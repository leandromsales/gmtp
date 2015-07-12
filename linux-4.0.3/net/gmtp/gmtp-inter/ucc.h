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
#define GMTP_THETA(X) DIV_ROUND_CLOSEST(X * 20, 100) /* X*0.02 */

/** gmtp-ucc. */
unsigned int gmtp_rtt_average(void);
unsigned int gmtp_rx_rate(void);
unsigned int gmtp_relay_queue_size(void);
unsigned int gmtp_get_current_rx_rate(void);
void gmtp_ucc(unsigned int h_user, unsigned char debug);

#endif /* UCC_H_ */
