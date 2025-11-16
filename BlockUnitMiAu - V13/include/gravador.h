#ifndef GRAVADOR_H
#define GRAVADOR_H

#include <Arduino.h>
#include <SD.h>

void gravador_init();
void gravador_start_recording(const char* filename);
void gravador_stop_recording();
bool gravador_is_recording();
void gravador_generate_and_record();

#endif