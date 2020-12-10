/*
 * speaker.h
 *
 *  Created on: Dec 4, 2020
 *      Author: afrid
 */

#ifndef SPEAKER_H_
#define SPEAKER_H_

#define SPEAKER_MASK 128

#define WAIT_TIME 100000

void initSpeaker(void);
void setFrequency(uint32_t frequency);

#endif /* SPEAKER_H_ */
