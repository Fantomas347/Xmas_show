#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mpg123.h>
#include <syslog.h>

// Minimum buffer time in milliseconds before signaling data available
#define MIN_BUFFER_MS 100

// WAV header structures
#pragma pack(push, 1)
typedef struct {
    char     riff_id[4];
    uint32_t riff_size;
    char     wave_id[4];
} RiffHeader;

typedef struct {
    char     chunk_id[4];
    uint32_t chunk_size;
} ChunkHeader;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} FmtChunk;
#pragma pack(pop)

static int mpg123_initialized = 0;

static AudioFormat detect_format(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return AUDIO_FORMAT_UNKNOWN;

    if (strcasecmp(ext, ".mp3") == 0) return AUDIO_FORMAT_MP3;
    if (strcasecmp(ext, ".wav") == 0) return AUDIO_FORMAT_WAV;

    return AUDIO_FORMAT_UNKNOWN;
}

static int open_wav(AudioStream *stream, const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open WAV");
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return -1;
    }

    size_t file_size = st.st_size;
    void *mapping = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapping == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    uint8_t *p = (uint8_t *)mapping;
    RiffHeader *riff = (RiffHeader *)p;

    if (memcmp(riff->riff_id, "RIFF", 4) != 0 ||
        memcmp(riff->wave_id, "WAVE", 4) != 0) {
        fprintf(stderr, "Not a RIFF/WAVE file\n");
        munmap(mapping, file_size);
        return -1;
    }

    p += sizeof(RiffHeader);

    FmtChunk fmt = {0};
    uint32_t data_size = 0;
    uint8_t *data_ptr = NULL;

    while (p < (uint8_t *)mapping + file_size) {
        ChunkHeader *ch = (ChunkHeader *)p;

        if (memcmp(ch->chunk_id, "fmt ", 4) == 0) {
            memcpy(&fmt, p + sizeof(ChunkHeader), sizeof(FmtChunk));
        } else if (memcmp(ch->chunk_id, "data", 4) == 0) {
            data_size = ch->chunk_size;
            data_ptr = p + sizeof(ChunkHeader);
            break;
        }

        p += sizeof(ChunkHeader) + ch->chunk_size;
    }

    if (!data_ptr) {
        fprintf(stderr, "No data chunk found\n");
        munmap(mapping, file_size);
        return -1;
    }

    if (fmt.audio_format != 1 || fmt.bits_per_sample != 16) {
        fprintf(stderr, "Unsupported WAV format (need PCM 16-bit)\n");
        munmap(mapping, file_size);
        return -1;
    }

    stream->format = AUDIO_FORMAT_WAV;
    stream->sample_rate = fmt.sample_rate;
    stream->channels = fmt.num_channels;
    stream->total_frames = data_size / (fmt.num_channels * 2);
    stream->mapping = mapping;
    stream->mapping_size = file_size;
    stream->wav_pcm = (int16_t *)data_ptr;
    stream->wav_frames_read = 0;

    // Lock WAV data into RAM for real-time playback
    if (mlock(mapping, file_size) != 0) {
        perror("mlock WAV (continuing anyway)");
    }

    return 0;
}

static int open_mp3(AudioStream *stream, const char *filename) {
    if (!mpg123_initialized) {
        if (mpg123_init() != MPG123_OK) {
            fprintf(stderr, "mpg123_init failed\n");
            return -1;
        }
        mpg123_initialized = 1;
    }

    int err;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    if (!mh) {
        fprintf(stderr, "mpg123_new: %s\n", mpg123_plain_strerror(err));
        return -1;
    }

    // Force output format: 16-bit signed, stereo, 44100Hz
    mpg123_param(mh, MPG123_FLAGS, MPG123_FORCE_STEREO, 0);
    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_QUIET, 0);

    if (mpg123_open(mh, filename) != MPG123_OK) {
        fprintf(stderr, "mpg123_open: %s\n", mpg123_strerror(mh));
        mpg123_delete(mh);
        return -1;
    }

    // Get format
    long rate;
    int channels, encoding;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        fprintf(stderr, "mpg123_getformat: %s\n", mpg123_strerror(mh));
        mpg123_close(mh);
        mpg123_delete(mh);
        return -1;
    }

    // Ensure 16-bit output
    mpg123_format_none(mh);
    mpg123_format(mh, rate, MPG123_STEREO, MPG123_ENC_SIGNED_16);

    // Get length if available
    off_t length = mpg123_length(mh);

    stream->format = AUDIO_FORMAT_MP3;
    stream->sample_rate = (uint32_t)rate;
    stream->channels = 2;  // Forced stereo
    stream->total_frames = (length > 0) ? (size_t)length : 0;
    stream->decoder_handle = mh;

    return 0;
}

AudioStream *audio_open(const char *filename) {
    AudioFormat fmt = detect_format(filename);
    if (fmt == AUDIO_FORMAT_UNKNOWN) {
        fprintf(stderr, "Unknown audio format: %s\n", filename);
        return NULL;
    }

    AudioStream *stream = calloc(1, sizeof(AudioStream));
    if (!stream) {
        perror("calloc AudioStream");
        return NULL;
    }

    stream->fd = -1;

    int result;
    if (fmt == AUDIO_FORMAT_WAV) {
        result = open_wav(stream, filename);
    } else {
        result = open_mp3(stream, filename);
    }

    if (result < 0) {
        free(stream);
        return NULL;
    }

    // Allocate ring buffer for MP3 streaming
    if (stream->format == AUDIO_FORMAT_MP3) {
        stream->ring_size = RING_BUFFER_SAMPLES;
        stream->ring_buffer = calloc(stream->ring_size, sizeof(int16_t));
        if (!stream->ring_buffer) {
            perror("calloc ring_buffer");
            audio_close(stream);
            return NULL;
        }

        pthread_mutex_init(&stream->mutex, NULL);
        pthread_cond_init(&stream->cond_space, NULL);
        pthread_cond_init(&stream->cond_data, NULL);
    }

    return stream;
}

// Decoder thread for MP3
static void *mp3_decoder_thread(void *arg) {
    AudioStream *stream = (AudioStream *)arg;
    mpg123_handle *mh = (mpg123_handle *)stream->decoder_handle;

    // Decode buffer (decode in chunks) - 100ms worth based on sample rate
    const size_t decode_samples = (stream->sample_rate / 10) * stream->channels;
    int16_t *decode_buf = malloc(decode_samples * sizeof(int16_t));
    if (!decode_buf) {
        stream->error = 1;
        stream->finished = 1;
        return NULL;
    }

    while (!stream->finished && !stream->error) {
        size_t done = 0;
        int ret = mpg123_read(mh, (unsigned char *)decode_buf,
                              decode_samples * sizeof(int16_t), &done);

        if (ret == MPG123_DONE || done == 0) {
            stream->finished = 1;
            pthread_cond_signal(&stream->cond_data);
            break;
        }

        if (ret != MPG123_OK && ret != MPG123_NEW_FORMAT) {
            syslog(LOG_ERR, "mpg123_read error: %s", mpg123_strerror(mh));
            stream->error = 1;
            stream->finished = 1;
            pthread_cond_signal(&stream->cond_data);
            break;
        }

        size_t samples_decoded = done / sizeof(int16_t);
        size_t samples_written = 0;

        while (samples_written < samples_decoded && !stream->error) {
            pthread_mutex_lock(&stream->mutex);

            // Calculate available space in ring buffer
            size_t write_pos = stream->write_pos;
            size_t read_pos = stream->read_pos;
            size_t used = (write_pos >= read_pos) ?
                          (write_pos - read_pos) :
                          (stream->ring_size - read_pos + write_pos);
            size_t space = stream->ring_size - used - 1;

            // Wait if buffer is full
            while (space < 2 && !stream->error) {
                pthread_cond_wait(&stream->cond_space, &stream->mutex);
                write_pos = stream->write_pos;
                read_pos = stream->read_pos;
                used = (write_pos >= read_pos) ?
                       (write_pos - read_pos) :
                       (stream->ring_size - read_pos + write_pos);
                space = stream->ring_size - used - 1;
            }

            if (stream->error) {
                pthread_mutex_unlock(&stream->mutex);
                break;
            }

            // Write samples to ring buffer
            size_t to_write = samples_decoded - samples_written;
            if (to_write > space) to_write = space;

            size_t first_chunk = stream->ring_size - write_pos;
            if (first_chunk > to_write) first_chunk = to_write;

            memcpy(&stream->ring_buffer[write_pos],
                   &decode_buf[samples_written],
                   first_chunk * sizeof(int16_t));

            if (to_write > first_chunk) {
                memcpy(&stream->ring_buffer[0],
                       &decode_buf[samples_written + first_chunk],
                       (to_write - first_chunk) * sizeof(int16_t));
            }

            stream->write_pos = (write_pos + to_write) % stream->ring_size;
            samples_written += to_write;

            // Signal data available
            pthread_cond_signal(&stream->cond_data);
            pthread_mutex_unlock(&stream->mutex);
        }
    }

    free(decode_buf);
    return NULL;
}

int audio_start(AudioStream *stream) {
    if (!stream) return -1;

    if (stream->format == AUDIO_FORMAT_MP3) {
        stream->thread_running = 1;
        if (pthread_create(&stream->decoder_thread, NULL,
                           mp3_decoder_thread, stream) != 0) {
            perror("pthread_create decoder");
            stream->thread_running = 0;
            return -1;
        }

        // Wait for initial buffer fill (MIN_BUFFER_MS worth of frames)
        size_t min_buffer_frames = (stream->sample_rate * MIN_BUFFER_MS) / 1000;
        pthread_mutex_lock(&stream->mutex);
        while (audio_available(stream) < min_buffer_frames &&
               !stream->finished && !stream->error) {
            pthread_cond_wait(&stream->cond_data, &stream->mutex);
        }
        pthread_mutex_unlock(&stream->mutex);
    }

    return 0;
}

int audio_read(AudioStream *stream, int16_t *buffer, size_t frames) {
    if (!stream) return -1;

    size_t samples_needed = frames * stream->channels;

    if (stream->format == AUDIO_FORMAT_WAV) {
        // Direct read from mmap'd WAV
        size_t frames_left = stream->total_frames - stream->wav_frames_read;
        if (frames_left == 0) return -1;  // Finished

        size_t to_read = (frames < frames_left) ? frames : frames_left;
        size_t samples = to_read * stream->channels;

        memcpy(buffer,
               &stream->wav_pcm[stream->wav_frames_read * stream->channels],
               samples * sizeof(int16_t));

        stream->wav_frames_read += to_read;
        return (int)to_read;
    }

    // MP3: read from ring buffer
    pthread_mutex_lock(&stream->mutex);

    size_t write_pos = stream->write_pos;
    size_t read_pos = stream->read_pos;
    size_t available = (write_pos >= read_pos) ?
                       (write_pos - read_pos) :
                       (stream->ring_size - read_pos + write_pos);

    if (available == 0) {
        pthread_mutex_unlock(&stream->mutex);
        if (stream->finished) return -1;
        return 0;  // Buffer empty, try again
    }

    size_t to_read = samples_needed;
    if (to_read > available) to_read = available;

    // Align to frame boundary
    to_read = (to_read / stream->channels) * stream->channels;

    size_t first_chunk = stream->ring_size - read_pos;
    if (first_chunk > to_read) first_chunk = to_read;

    memcpy(buffer, &stream->ring_buffer[read_pos],
           first_chunk * sizeof(int16_t));

    if (to_read > first_chunk) {
        memcpy(&buffer[first_chunk], &stream->ring_buffer[0],
               (to_read - first_chunk) * sizeof(int16_t));
    }

    stream->read_pos = (read_pos + to_read) % stream->ring_size;

    // Signal space available to decoder
    pthread_cond_signal(&stream->cond_space);
    pthread_mutex_unlock(&stream->mutex);

    return (int)(to_read / stream->channels);
}

int audio_finished(AudioStream *stream) {
    if (!stream) return 1;

    if (stream->format == AUDIO_FORMAT_WAV) {
        return stream->wav_frames_read >= stream->total_frames;
    }

    return stream->finished && audio_available(stream) == 0;
}

size_t audio_available(AudioStream *stream) {
    if (!stream) return 0;

    if (stream->format == AUDIO_FORMAT_WAV) {
        return stream->total_frames - stream->wav_frames_read;
    }

    size_t write_pos = stream->write_pos;
    size_t read_pos = stream->read_pos;
    size_t samples = (write_pos >= read_pos) ?
                     (write_pos - read_pos) :
                     (stream->ring_size - read_pos + write_pos);

    return samples / stream->channels;
}

void audio_close(AudioStream *stream) {
    if (!stream) return;

    // Stop decoder thread
    if (stream->thread_running) {
        stream->error = 1;  // Signal thread to stop
        pthread_cond_signal(&stream->cond_space);
        pthread_join(stream->decoder_thread, NULL);
        stream->thread_running = 0;
    }

    // Close format-specific resources
    if (stream->format == AUDIO_FORMAT_MP3 && stream->decoder_handle) {
        mpg123_close((mpg123_handle *)stream->decoder_handle);
        mpg123_delete((mpg123_handle *)stream->decoder_handle);
    }

    if (stream->mapping) {
        munmap(stream->mapping, stream->mapping_size);
    }

    if (stream->ring_buffer) {
        pthread_mutex_destroy(&stream->mutex);
        pthread_cond_destroy(&stream->cond_space);
        pthread_cond_destroy(&stream->cond_data);
        free(stream->ring_buffer);
    }

    free(stream);
}
