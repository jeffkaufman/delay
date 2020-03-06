#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portaudio.h"

#define SAMPLE_RATE       (44100)
#define FRAMES_PER_BUFFER   (128)  // minimize latency
#define DITHER_FLAG           (0)

/* Select sample format. */
#define SAMPLE_SIZE (4)
#define SAMPLE_SILENCE  (0.0f)

#define MAX_DELAY_SECONDS (4)
#define HISTORY_SIZE (SAMPLE_RATE*MAX_DELAY_SECONDS)

float history[HISTORY_SIZE];
int history_index;

float bpm;
float decay;

float nextSample(float in) {
  int delay_samples = (int)(60*SAMPLE_RATE/bpm);
  int old_index = (HISTORY_SIZE + history_index - delay_samples) % HISTORY_SIZE;
  float wet = history[old_index]; 
  history[history_index] = in + (wet*decay);
  history_index = (history_index + 1) % HISTORY_SIZE;
  return wet;

  /*


  return in;
  int old_index = (HISTORY_SIZE + buffer_index - delay_samples) % HISTORY_SIZE;
  float wet = buffer[old_index];
  buffer[buffer_index] = in; //in + wet * decay;
  buffer_index = (buffer_index + 1) % HISTORY_SIZE;
  if (buffer_index % 1000 == 0) {
    printf("%d ... %d [%d]: %.4f -> %.4f\n", buffer_index, old_index,
           HISTORY_SIZE, in, wet);
  }
  return wet;*/
}

int main(void) {
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream = NULL;
    PaError err;
    const PaDeviceInfo* inputInfo;
    const PaDeviceInfo* outputInfo;
    float *sampleBlock = NULL;
    int numBytes;
  
    history_index = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
      history[i] = SAMPLE_SILENCE;
    }    

    bpm = 120/2;
    decay = 0.15;

    printf("patest_read_write_wire.c\n"); fflush(stdout);
    printf("sizeof(int) = %lu\n", sizeof(int)); fflush(stdout);
    printf("sizeof(long) = %lu\n", sizeof(long)); fflush(stdout);

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    int numDevices = Pa_GetDeviceCount();
    const PaDeviceInfo* deviceInfo;
    int best_audio_device_index = -1;
    for(int i = 0; i < numDevices; i++) {
      deviceInfo = Pa_GetDeviceInfo(i);
      printf("device[%d]: %s\n", i, deviceInfo->name);
      if (strcmp(deviceInfo->name, "USB Audio Device") == 0) {
        best_audio_device_index = i;
      }
    }

    if (best_audio_device_index == -1) {
      best_audio_device_index = Pa_GetDefaultInputDevice();
    }

    inputParameters.device = best_audio_device_index;
    printf( "Input device # %d.\n", inputParameters.device );
    inputInfo = Pa_GetDeviceInfo( inputParameters.device );
    printf( "    Name: %s\n", inputInfo->name );
    printf( "      LL: %.3g ms\n", inputInfo->defaultLowInputLatency*1000 );

    outputParameters.device = best_audio_device_index;
    printf( "Output device # %d.\n", outputParameters.device );
    outputInfo = Pa_GetDeviceInfo( outputParameters.device );
    printf( "   Name: %s\n", outputInfo->name );
    printf( "     LL: %.3g ms\n", outputInfo->defaultLowOutputLatency*1000 );

    inputParameters.channelCount = 1;  // mono
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = inputInfo->defaultLowInputLatency ;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.channelCount = 1;  // mono
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = outputInfo->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    /* -- setup -- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              NULL, /* no callback, use blocking API */
              NULL ); /* no callback, so no callback userData */
    if( err != paNoError ) goto error;

    numBytes = FRAMES_PER_BUFFER * SAMPLE_SIZE ;
    sampleBlock = (float *) malloc( numBytes );
    if( sampleBlock == NULL )
    {
        printf("Could not allocate record array.\n");
        goto error;
    }
    memset( sampleBlock, SAMPLE_SILENCE, numBytes );

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

    while(1) {
        // You may get underruns or overruns if the output is not primed by PortAudio.
        Pa_WriteStream( stream, sampleBlock, FRAMES_PER_BUFFER );
        Pa_ReadStream( stream, sampleBlock, FRAMES_PER_BUFFER );
        for (int i = 0; i < FRAMES_PER_BUFFER; i++) {
          sampleBlock[i] = nextSample(sampleBlock[i]);
        }
    }

 error:
    if( stream ) {
       Pa_AbortStream( stream );
       Pa_CloseStream( stream );
    }
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}

