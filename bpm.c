#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sndfile.h>
#include <windows.h>

#define WINDOW_SIZE 1024
#define HOP_SIZE 512
#define MIN_BPM 60
#define MAX_BPM 200
#define SAMPLE_RATE 44100

// Function to calculate the energy of a frame
double calculate_energy(const double* frame, int size) {
    double energy = 0.0;
    for (int i = 0; i < size; i++) {
        energy += frame[i] * frame[i];
    }
    return energy;
}

// Function to detect onset from energy difference
void detect_onsets(const double* energy, int size, int* onset_positions, int* num_onsets) {
    double mean = 0.0;
    double std_dev = 0.0;
    
    // Calculate mean
    for (int i = 0; i < size; i++) {
        mean += energy[i];
    }
    mean /= size;
    
    // Calculate standard deviation
    for (int i = 0; i < size; i++) {
        std_dev += (energy[i] - mean) * (energy[i] - mean);
    }
    std_dev = sqrt(std_dev / size);
    
    // Detect onsets using adaptive thresholding
    double threshold = mean + 1.5 * std_dev;
    *num_onsets = 0;
    
    for (int i = 1; i < size - 1; i++) {
        if (energy[i] > threshold && energy[i] > energy[i-1] && energy[i] > energy[i+1]) {
            onset_positions[*num_onsets] = i;
            (*num_onsets)++;
        }
    }
}

// Function to estimate BPM from onset positions
int estimate_bpm(const int* onset_positions, int num_onsets, int hop_size, int sample_rate) {
    if (num_onsets < 2) return 0;
    
    // Calculate inter-onset intervals
    int max_intervals = 1000;
    double* intervals = (double*)malloc(max_intervals * sizeof(double));
    int interval_count = 0;
    
    for (int i = 1; i < num_onsets && interval_count < max_intervals; i++) {
        double interval = (onset_positions[i] - onset_positions[i-1]) * (double)hop_size / sample_rate;
        if (interval > 0.1 && interval < 2.0) {  // Filter reasonable intervals (0.1s to 2s)
            intervals[interval_count++] = interval;
        }
    }
    
    if (interval_count == 0) {
        free(intervals);
        return 0;
    }
    
    // Find median interval
    for (int i = 0; i < interval_count - 1; i++) {
        for (int j = 0; j < interval_count - i - 1; j++) {
            if (intervals[j] > intervals[j + 1]) {
                double temp = intervals[j];
                intervals[j] = intervals[j + 1];
                intervals[j + 1] = temp;
            }
        }
    }
    
    double median_interval = intervals[interval_count / 2];
    free(intervals);
    
    // Convert to BPM
    int bpm = (int)round(60.0 / median_interval);
    
    // Ensure BPM is within reasonable range
    while (bpm < MIN_BPM && bpm > 0) bpm *= 2;
    while (bpm > MAX_BPM) bpm /= 2;
    
    return bpm;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <audio_file_path>\n", argv[0]);
        return 1;
    }

    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    SNDFILE* file = sf_open(argv[1], SFM_READ, &sf_info);
    
    if (!file) {
        printf("Error opening file: %s\n", sf_strerror(NULL));
        return 1;
    }

    // Allocate buffer for audio data
    double* buffer = (double*)malloc(sf_info.frames * sizeof(double));
    if (!buffer) {
        printf("Memory allocation failed\n");
        sf_close(file);
        return 1;
    }

    // Read audio data
    sf_count_t frames_read = sf_readf_double(file, buffer, sf_info.frames);
    sf_close(file);

    // Convert stereo to mono if necessary
    if (sf_info.channels == 2) {
        for (sf_count_t i = 0; i < frames_read; i++) {
            buffer[i] = (buffer[i * 2] + buffer[i * 2 + 1]) / 2.0;
        }
    }

    // Calculate number of frames
    int num_frames = (frames_read - WINDOW_SIZE) / HOP_SIZE;
    double* energy = (double*)malloc(num_frames * sizeof(double));
    
    // Calculate energy for each frame
    #pragma omp parallel for
    for (int i = 0; i < num_frames; i++) {
        energy[i] = calculate_energy(buffer + i * HOP_SIZE, WINDOW_SIZE);
    }

    // Detect onsets
    int* onset_positions = (int*)malloc(num_frames * sizeof(int));
    int num_onsets = 0;
    detect_onsets(energy, num_frames, onset_positions, &num_onsets);

    // Estimate BPM
    int bpm = estimate_bpm(onset_positions, num_onsets, HOP_SIZE, sf_info.samplerate);

    printf("Estimated BPM: %d\n", bpm);

    // Clean up
    free(buffer);
    free(energy);
    free(onset_positions);

    return 0;
}