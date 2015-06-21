/*
 * ucc.h
 *
 *  Created on: 14/05/2015
 *      Author: wendell
 */

#ifndef UCC_H_
#define UCC_H_

#define ALPHA(X) (((X)*3)/10)  /* X*0.3 */
#define BETA(X)  (((X)*6)/10)  /* X*0.6 */
#define GHAMA(X) (((X)*10)/10) /* X*1.0 */
#define THETA(X) (((X)*2)/100) /* X*0.02 */

#define MOD(X)   ((X > 0) ? (X) : -(X))

/** gmtp-ucc. */
unsigned int gmtp_rtt_average(void);
unsigned int gmtp_rx_rate(void);
unsigned int gmtp_relay_queue_size(void);
unsigned int gmtp_get_current_rx_rate(void);
void gmtp_ucc(unsigned int h_user);

#endif /* UCC_H_ */
