/*
 * gmtp.h
 *
 *  Created on: 18/06/2014
 *      Author: Wendell Silva Soares <wendell@compelab.org>
 */

#ifndef GMTP_H_
#define GMTP_H_

#include "include/linux/gmtp.h"

void gmtp_close(struct sock *sk, long timeout);

#endif /* GMTP_H_ */
