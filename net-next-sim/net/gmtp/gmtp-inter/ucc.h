/*
 * ucc.h
 *
 *  Created on: 14/05/2015
 *      Author: wendell
 */

#ifndef UCC_H_
#define UCC_H_

#define GMTP_ALPHA(X) DIV_ROUND_CLOSEST(X * 30, 100) /* X*0.3 */
#define GMTP_BETA(X)  DIV_ROUND_CLOSEST(X * 60, 100) /* X*0.6 */
#define GMTP_GHAMA(X) (X) /* DIV_ROUND_CLOSEST(X * 100, 100)*/ /* X*1.0 */
#define GMTP_THETA(X) DIV_ROUND_CLOSEST(X * 2000, 100000) /* X*0.02 */
#define GMTP_ONE_MINUS_THETA(X) DIV_ROUND_CLOSEST(X * 98000, 100000) /* X*(1-0.02) */

enum gmtp_ucc_log_level {
	GMTP_UCC_NONE,
	GMTP_UCC_OUTPUT,
	GMTP_UCC_DEBUG,
	GMTP_UCC_ALL
};

/** gmtp-ucc. */
void gmtp_ucc_callback(unsigned long);
unsigned int gmtp_relay_queue_size(void);
void gmtp_ucc(enum gmtp_ucc_log_level log_level);

#endif /* UCC_H_ */
