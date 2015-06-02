/*
 * modem_errors.h
 *
 *  Created on: Jun 2, 2015
 *      Author: sergioam
 */

#ifndef MODEM_ERRORS_H_
#define MODEM_ERRORS_H_

#define NO_ERROR 0
#define ERROR_SIM_NOT_INSERTED 310

/*
CME ERROR (GSM Equipment Related errors)
 Print	 Email
Parent Category: FAQ
Hits: 25088
CME ERROR: 0	Phone failure
CME ERROR: 1	No connection to phone
CME ERROR: 2	Phone adapter link reserved
CME ERROR: 3	Operation not allowed
CME ERROR: 4	Operation not supported
CME ERROR: 5	PH_SIM PIN required
CME ERROR: 6	PH_FSIM PIN required
CME ERROR: 7	PH_FSIM PUK required
CME ERROR: 10	SIM not inserted
CME ERROR: 11	SIM PIN required
CME ERROR: 12	SIM PUK required
CME ERROR: 13	SIM failure
CME ERROR: 14	SIM busy
CME ERROR: 15	SIM wrong
CME ERROR: 16	Incorrect password
CME ERROR: 17	SIM PIN2 required
CME ERROR: 18	SIM PUK2 required
CME ERROR: 20	Memory full
CME ERROR: 21	Invalid index
CME ERROR: 22	Not found
CME ERROR: 23	Memory failure
CME ERROR: 24	Text string too long
CME ERROR: 25	Invalid characters in text string
CME ERROR: 26	Dial string too long
CME ERROR: 27	Invalid characters in dial string
CME ERROR: 30	No network service
CME ERROR: 31	Network timeout
CME ERROR: 32	Network not allowed, emergency calls only
CME ERROR: 40	Network personalization PIN required
CME ERROR: 41	Network personalization PUK required
CME ERROR: 42	Network subset personalization PIN required
CME ERROR: 43	Network subset personalization PUK required
CME ERROR: 44	Service provider personalization PIN required
CME ERROR: 45	Service provider personalization PUK required
CME ERROR: 46	Corporate personalization PIN required
CME ERROR: 47	Corporate personalization PUK required
CME ERROR: 48	PH-SIM PUK required
CME ERROR: 100	Unknown error
CME ERROR: 103	Illegal MS
CME ERROR: 106	Illegal ME
CME ERROR: 107	GPRS services not allowed
CME ERROR: 111	PLMN not allowed
CME ERROR: 112	Location area not allowed
CME ERROR: 113	Roaming not allowed in this location area
CME ERROR: 126	Operation temporary not allowed
CME ERROR: 132	Service operation not supported
CME ERROR: 133	Requested service option not subscribed
CME ERROR: 134	Service option temporary out of order
CME ERROR: 148	Unspecified GPRS error
CME ERROR: 149	PDP authentication failure
CME ERROR: 150	Invalid mobile class
CME ERROR: 256	Operation temporarily not allowed
CME ERROR: 257	Call barred
CME ERROR: 258	Phone is busy
CME ERROR: 259	User abort
CME ERROR: 260	Invalid dial string
CME ERROR: 261	SS not executed
CME ERROR: 262	SIM Blocked
CME ERROR: 263	Invalid block
CME ERROR: 772	SIM powered down

CMS ERROR (GSM Network Related errors)

CMS ERROR: 1	Unassigned number
CMS ERROR: 8	Operator determined barring
CMS ERROR: 10	Call bared
CMS ERROR: 21	Short message transfer rejected
CMS ERROR: 27	Destination out of service
CMS ERROR: 28	Unindentified subscriber
CMS ERROR: 29	Facility rejected
CMS ERROR: 30	Unknown subscriber
CMS ERROR: 38	Network out of order
CMS ERROR: 41	Temporary failure
CMS ERROR: 42	Congestion
CMS ERROR: 47	Recources unavailable
CMS ERROR: 50	Requested facility not subscribed
CMS ERROR: 69	Requested facility not implemented
CMS ERROR: 81	Invalid short message transfer reference value
CMS ERROR: 95	Invalid message unspecified
CMS ERROR: 96	Invalid mandatory information
CMS ERROR: 97	Message type non existent or not implemented
CMS ERROR: 98	Message not compatible with short message protocol
CMS ERROR: 99	Information element non-existent or not implemente
CMS ERROR: 111	Protocol error, unspecified
CMS ERROR: 127	Internetworking , unspecified
CMS ERROR: 128	Telematic internetworking not supported
CMS ERROR: 129	Short message type 0 not supported
CMS ERROR: 130	Cannot replace short message
CMS ERROR: 143	Unspecified TP-PID error
CMS ERROR: 144	Data code scheme not supported
CMS ERROR: 145	Message class not supported
CMS ERROR: 159	Unspecified TP-DCS error
CMS ERROR: 160	Command cannot be actioned
CMS ERROR: 161	Command unsupported
CMS ERROR: 175	Unspecified TP-Command error
CMS ERROR: 176	TPDU not supported
CMS ERROR: 192	SC busy
CMS ERROR: 193	No SC subscription
CMS ERROR: 194	SC System failure
CMS ERROR: 195	Invalid SME address
CMS ERROR: 196	Destination SME barred
CMS ERROR: 197	SM Rejected-Duplicate SM
CMS ERROR: 198	TP-VPF not supported
CMS ERROR: 199	TP-VP not supported
CMS ERROR: 208	D0 SIM SMS Storage full
CMS ERROR: 209	No SMS Storage capability in SIM
CMS ERROR: 210	Error in MS
CMS ERROR: 211	Memory capacity exceeded
CMS ERROR: 212	Sim application toolkit busy
CMS ERROR: 213	SIM data download error
CMS ERROR: 255	Unspecified error cause
CMS ERROR: 300	ME Failure
CMS ERROR: 301	SMS service of ME reserved
CMS ERROR: 302	Operation not allowed
CMS ERROR: 303	Operation not supported
CMS ERROR: 304	Invalid PDU mode parameter
CMS ERROR: 305	Invalid Text mode parameter
CMS ERROR: 310	SIM not inserted
CMS ERROR: 311	SIM PIN required
CMS ERROR: 312	PH-SIM PIN required
CMS ERROR: 313	SIM failure
CMS ERROR: 314	SIM busy
CMS ERROR: 315	SIM wrong
CMS ERROR: 316	SIM PUK required
CMS ERROR: 317	SIM PIN2 required
CMS ERROR: 318	SIM PUK2 required
CMS ERROR: 320	Memory failure
CMS ERROR: 321	Invalid memory index
CMS ERROR: 322	Memory full
CMS ERROR: 330	SMSC address unknown
CMS ERROR: 331	No network service
CMS ERROR: 332	Network timeout
CMS ERROR: 340	No +CNMA expected
CMS ERROR: 500	Unknown error
CMS ERROR: 512	User abort (spécifique a certain fabricants)
CMS ERROR: 513	Unable to store
CMS ERROR: 514	Invalid Status
CMS ERROR: 515	Device busy or Invalid Character in string
CMS ERROR: 516	Invalid length
CMS ERROR: 517	Invalid character in PDU
CMS ERROR: 518	Invalid parameter
CMS ERROR: 519	Invalid length or character
CMS ERROR: 520	Invalid character in text
CMS ERROR: 521	Timer expired
CMS ERROR: 522	Operation temporary not allowed
CMS ERROR: 532	SIM not ready
CMS ERROR: 534	Cell Broadcast error unknown
CMS ERROR: 535	Protocol stack busy
CMS ERROR: 538	Invalid parameter
*/

#endif /* MODEM_ERRORS_H_ */
