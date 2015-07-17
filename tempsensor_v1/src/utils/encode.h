/*
 * encode.h
 *
 *  Created on: 18 Jun 2015
 *      Author: dancat
 */

#ifndef ENCODE_H_
#define ENCODE_H_

#define MAX_ENCODED_LINE_SIZE 40

void encode(float inputVal, char* outputChars);
void encode_string(char* inputString, char* outputString, char* delimiter);

#endif /* ENCODE_H_ */
