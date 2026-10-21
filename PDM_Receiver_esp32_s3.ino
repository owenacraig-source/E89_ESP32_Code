#include <Arduino.h>
#include <math.h>

#include "PdmCapture.h"


// Application buffer holding one 10 ms PCM block.
static AudioBlock audio;

// Counters used to monitor capture continuity.
static uint32_t receivedBlocks = 0;
static uint32_t missingBlocks = 0;
static uint32_t expectedSequence = 0;
static bool haveSequence = false;

// Timestamp of the last diagnostic message.
static uint32_t lastReportMs = 0;


void setup()
{
    // Serial is for status messages, not continuous audio streaming.
    Serial.begin(115200);
    delay(1000);

    Serial.println("ESP32-S3 PDM-to-PCM capture");

    // The shared implementation also uses I2S0 on the ESP32-S3.
    //
    // The hardware generates the microphone clock and converts
    // the received PDM stream into 16-bit PCM.
    beginPdmCapture(
        GPIO_NUM_4,  // ESP32-S3 output -> microphone CLK
        GPIO_NUM_5,  // ESP32-S3 input  <- microphone DATA
        false        // Left slot; change to true for right slot
    );

    Serial.println("PCM: 48 kHz, signed 16-bit, mono");
    Serial.println("PDM clock: nominal 3.072 MHz");
}


void loop()
{
    // Copy the next queued block into the application buffer.
    //
    // If no block becomes available within one second, report
    // the timeout and try again on the next loop iteration.
    if (!readAudioBlock(audio, 1000)) {
        Serial.println("Timed out waiting for audio");
        return;
    }

    // Detect gaps in the capture sequence.
    //
    // A gap means one or more capture blocks were discarded.
    if (haveSequence) {
        missingBlocks += audio.sequence - expectedSequence;
    }

    expectedSequence = audio.sequence + 1;
    haveSequence = true;
    receivedBlocks++;

    // The PCM data is now available in audio.samples[].
    //
    // Add your band-pass filtering here.
    //
    // If using floating-point filtering, a normalized input is:
    //
    //     float x = (float)audio.samples[i] / 32768.0f;
    //
    // Filter state must persist from one block to the next.


    // Calculate the latest block's uncalibrated RMS.
    //
    // This is an example of consuming the buffered audio, not a
    // complete sound pressure level measurement.
    double sumSquares = 0.0;

    for (size_t i = 0; i < SAMPLES_PER_BLOCK; ++i) {
        double sample = audio.samples[i];

        sumSquares += sample * sample;
    }

    double rms = sqrt(sumSquares / SAMPLES_PER_BLOCK);

    // Report approximately once per second.
    //
    // Keep serial output sparse so it does not slow processing
    // enough to fill the audio queue.
    if (millis() - lastReportMs >= 1000) {
        lastReportMs = millis();

        Serial.printf(
            "Blocks: %lu, missing: %lu, latest RMS: %.1f counts\n",
            (unsigned long)receivedBlocks,
            (unsigned long)missingBlocks,
            rms
        );
    }

    // The next successful readAudioBlock() call will overwrite
    // audio.samples[] with the next block.
}