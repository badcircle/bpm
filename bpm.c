#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpg123.h>
#include <windows.h>

#define WINDOW_SIZE 1024
#define HOP_SIZE 512
#define MIN_BPM 60
#define MAX_BPM 200

double calculate_energy(const float* frame, int size) {
    double energy = 0.0;
    for (int i = 0; i < size; i++) {
        energy += frame[i] * frame[i];
    }
    return energy / size;  // Normalize by frame size
}

void detect_onsets(const double* energy, int size, int* onset_positions, int* num_onsets) {
    // Calculate mean and std dev for adaptive thresholding
    double sum = 0.0, sum_sq = 0.0;
    double max_energy = 0.0;
    
    for (int i = 0; i < size; i++) {
        sum += energy[i];
        sum_sq += energy[i] * energy[i];
        if (energy[i] > max_energy) max_energy = energy[i];
    }
    
    double mean = sum / size;
    double variance = (sum_sq / size) - (mean * mean);
    double std_dev = sqrt(variance);
    
    // Use adaptive threshold
    double threshold = mean + 1.5 * std_dev;
    printf("Energy stats - Mean: %f, Std Dev: %f, Max: %f, Threshold: %f\n", 
           mean, std_dev, max_energy, threshold);
    
    *num_onsets = 0;
    int min_distance = (int)(0.05 * 44100 / HOP_SIZE);  // Minimum 50ms between onsets
    int last_onset = -min_distance;
    
    for (int i = 2; i < size - 2; i++) {
        if (energy[i] > threshold && 
            energy[i] > energy[i-1] && energy[i] > energy[i-2] &&
            energy[i] > energy[i+1] && energy[i] > energy[i+2] &&
            (i - last_onset) >= min_distance) {
            
            onset_positions[*num_onsets] = i;
            (*num_onsets)++;
            last_onset = i;
        }
    }
    
    printf("Found %d onsets\n", *num_onsets);
}

int estimate_bpm(const int* onset_positions, int num_onsets, int hop_size, int sample_rate) {
    if (num_onsets < 4) {
        printf("Too few onsets to estimate BPM\n");
        return 0;
    }
    
    // Calculate inter-onset intervals
    double* intervals = (double*)malloc((num_onsets - 1) * sizeof(double));
    int interval_count = 0;
    
    for (int i = 1; i < num_onsets; i++) {
        double interval = (onset_positions[i] - onset_positions[i-1]) * (double)hop_size / sample_rate;
        if (interval >= 0.2 && interval <= 2.0) {  // Accept intervals for 30-300 BPM
            intervals[interval_count++] = interval;
        }
    }
    
    if (interval_count < 3) {
        printf("Too few valid intervals to estimate BPM\n");
        free(intervals);
        return 0;
    }
    
    // Sort intervals
    for (int i = 0; i < interval_count - 1; i++) {
        for (int j = 0; j < interval_count - i - 1; j++) {
            if (intervals[j] > intervals[j + 1]) {
                double temp = intervals[j];
                intervals[j] = intervals[j + 1];
                intervals[j + 1] = temp;
            }
        }
    }
    
    // Use median interval
    double median_interval = intervals[interval_count / 2];
    printf("Median interval: %f seconds\n", median_interval);
    
    free(intervals);
    
    int bpm = (int)round(60.0 / median_interval);
    
    // Adjust to reasonable range
    while (bpm < MIN_BPM && bpm > 0) bpm *= 2;
    while (bpm > MAX_BPM) bpm /= 2;
    
    return bpm;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <mp3_file_path>\n", argv[0]);
        return 1;
    }

    // Initialize mpg123
    mpg123_init();
    int err = 0;
    mpg123_handle *mh = mpg123_new(NULL, &err);
    
    if (mh == NULL) {
        printf("Error initializing mpg123: %s\n", mpg123_plain_strerror(err));
        return 1;
    }

    // Set up format forcing
    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.0);
    mpg123_param(mh, MPG123_RESYNC_LIMIT, -1, 0.0);
    
    if (mpg123_open(mh, argv[1]) != MPG123_OK) {
        printf("Error opening file: %s\n", mpg123_strerror(mh));
        mpg123_delete(mh);
        return 1;
    }

    // Get the audio format
    long rate;
    int channels, encoding;
    mpg123_getformat(mh, &rate, &channels, &encoding);
    
    printf("Sample rate: %ld Hz\n", rate);
    printf("Channels: %d\n", channels);
    
    // Allocate buffers
    size_t buffer_size = mpg123_outblock(mh);
    float* buffer = (float*)malloc(buffer_size);
    float* audio_data = NULL;
    size_t audio_data_size = 0;
    size_t audio_data_capacity = buffer_size;
    
    audio_data = (float*)malloc(audio_data_capacity * sizeof(float));
    if (!audio_data) {
        printf("Failed to allocate audio buffer\n");
        return 1;
    }
    
    printf("Reading MP3 data...\n");
    
    // Read and decode the entire file
    size_t done;
    while (mpg123_read(mh, (unsigned char*)buffer, buffer_size, &done) == MPG123_OK) {
        size_t samples = done / sizeof(float);
        
        // Ensure we have enough space
        if (audio_data_size + samples/2 > audio_data_capacity) {
            audio_data_capacity *= 2;
            float* new_buffer = (float*)realloc(audio_data, audio_data_capacity * sizeof(float));
            if (!new_buffer) {
                printf("Failed to reallocate buffer\n");
                free(audio_data);
                return 1;
            }
            audio_data = new_buffer;
        }
        
        // Convert to mono while copying
        for (size_t i = 0; i < samples; i += channels) {
            float sum = 0;
            for (int c = 0; c < channels; c++) {
                sum += buffer[i + c];
            }
            audio_data[audio_data_size++] = sum / channels;
        }
    }
    
    printf("Read %zu mono samples\n", audio_data_size);
    
    // Calculate number of frames
    int num_frames = (audio_data_size - WINDOW_SIZE) / HOP_SIZE;
    printf("Number of analysis frames: %d\n", num_frames);
    
    // Calculate energy for each frame
    double* energy = (double*)malloc(num_frames * sizeof(double));
    printf("Calculating energy...\n");
    
    #pragma omp parallel for
    for (int i = 0; i < num_frames; i++) {
        energy[i] = calculate_energy(audio_data + i * HOP_SIZE, WINDOW_SIZE);
    }
    
    // Detect onsets
    int* onset_positions = (int*)malloc(num_frames * sizeof(int));
    int num_onsets = 0;
    
    printf("Detecting onsets...\n");
    detect_onsets(energy, num_frames, onset_positions, &num_onsets);
    
    // Calculate BPM
    printf("Calculating BPM...\n");
    int bpm = estimate_bpm(onset_positions, num_onsets, HOP_SIZE, rate);
    
    if (bpm > 0) {
        printf("\nEstimated BPM: %d\n", bpm);
    } else {
        printf("\nCould not determine BPM\n");
    }
    
    // Cleanup
    free(buffer);
    free(audio_data);
    free(energy);
    free(onset_positions);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    
    return 0;
}