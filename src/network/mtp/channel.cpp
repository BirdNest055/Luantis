// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <algorithm>
#include "network/mtp/internal.h"
#include "network/mtp/impl_constants.h"
#include "util/numeric.h"

namespace con
{

/*
	Channel
*/

u16 Channel::readNextIncomingSeqNum()
{
	MutexAutoLock internal(m_internal_mutex);
	return next_incoming_seqnum;
}

u16 Channel::incNextIncomingSeqNum()
{
	MutexAutoLock internal(m_internal_mutex);
	u16 retval = next_incoming_seqnum;
	next_incoming_seqnum++;
	return retval;
}

u16 Channel::readNextSplitSeqNum()
{
	MutexAutoLock internal(m_internal_mutex);
	return next_outgoing_split_seqnum;
}
void Channel::setNextSplitSeqNum(u16 seqnum)
{
	MutexAutoLock internal(m_internal_mutex);
	next_outgoing_split_seqnum = seqnum;
}

u16 Channel::getOutgoingSequenceNumber(bool& successful)
{
	MutexAutoLock internal(m_internal_mutex);

	u16 retval = next_outgoing_seqnum;
	successful = false;

	/* shortcut if there ain't any packet in outgoing list */
	if (outgoing_reliables_sent.empty()) {
		successful = true;
		next_outgoing_seqnum++;
		return retval;
	}

	u16 lowest_unacked_seqnumber;
	if (outgoing_reliables_sent.getFirstSeqnum(lowest_unacked_seqnumber)) {
		if (lowest_unacked_seqnumber < next_outgoing_seqnum) {
			// ugly cast but this one is required in order to tell compiler we
			// know about difference of two unsigned may be negative in general
			// but we already made sure it won't happen in this case
			if (((u16)(next_outgoing_seqnum - lowest_unacked_seqnumber)) > m_window_size) {
				return 0;
			}
		} else {
			// ugly cast but this one is required in order to tell compiler we
			// know about difference of two unsigned may be negative in general
			// but we already made sure it won't happen in this case
			if ((next_outgoing_seqnum + (u16)(SEQNUM_MAX - lowest_unacked_seqnumber)) >
					m_window_size) {
				return 0;
			}
		}
	}

	successful = true;
	next_outgoing_seqnum++;
	return retval;
}

u16 Channel::readOutgoingSequenceNumber()
{
	MutexAutoLock internal(m_internal_mutex);
	return next_outgoing_seqnum;
}

bool Channel::putBackSequenceNumber(u16 seqnum)
{
	if (((seqnum + 1) % (SEQNUM_MAX+1)) == next_outgoing_seqnum) {

		next_outgoing_seqnum = seqnum;
		return true;
	}
	return false;
}

void Channel::UpdateBytesSent(unsigned int bytes, unsigned int packets)
{
	MutexAutoLock internal(m_internal_mutex);
	current_bytes_transfered += bytes;
	current_packet_successful += packets;
}

void Channel::UpdateBytesReceived(unsigned int bytes) {
	MutexAutoLock internal(m_internal_mutex);
	current_bytes_received += bytes;
}

void Channel::UpdateBytesLost(unsigned int bytes)
{
	MutexAutoLock internal(m_internal_mutex);
	current_bytes_lost += bytes;
}


void Channel::UpdatePacketLossCounter(unsigned int count)
{
	MutexAutoLock internal(m_internal_mutex);
	current_packet_loss += count;
}

void Channel::UpdatePacketTooLateCounter()
{
	MutexAutoLock internal(m_internal_mutex);
	current_packet_too_late++;
}

void Channel::UpdateTimers(float dtime)
{
	bpm_counter += dtime;
	packet_loss_counter += dtime;

	if (packet_loss_counter > 1.0f) {
		packet_loss_counter -= 1.0f;

		unsigned int packet_loss;
		unsigned int packets_successful;
		unsigned int packet_too_late;

		bool reasonable_amount_of_data_transmitted = false;

		{
			MutexAutoLock internal(m_internal_mutex);
			packet_loss = current_packet_loss;
			packet_too_late = current_packet_too_late;
			packets_successful = current_packet_successful;

			// has half the window even been used?
			if (current_bytes_transfered > (unsigned int) (m_window_size*512/2)) {
				reasonable_amount_of_data_transmitted = true;
			}
			current_packet_loss = 0;
			current_packet_too_late = 0;
			current_packet_successful = 0;
		}

		// Packets too late means either packet duplication along the way
		// or we were too fast in resending it (which should be self-regulating).
		// Count this a signal of congestion, like packet loss.
		packet_loss = std::min(packet_loss + packet_too_late, packets_successful);

		/* dynamic window size */
		float successful_to_lost_ratio = 0.0f;
		bool done = false;

		if (packets_successful > 0) {
			successful_to_lost_ratio = packet_loss/packets_successful;
		} else if (packet_loss > 0) {
			setWindowSize(m_window_size - 10);
			done = true;
		}

		if (!done) {
			if (successful_to_lost_ratio < 0.01f) {
				/* don't even think about increasing if we didn't even
				 * use major parts of our window */
				if (reasonable_amount_of_data_transmitted)
					setWindowSize(m_window_size + 100);
			} else if (successful_to_lost_ratio < 0.05f) {
				/* don't even think about increasing if we didn't even
				 * use major parts of our window */
				if (reasonable_amount_of_data_transmitted)
					setWindowSize(m_window_size + 50);
			} else if (successful_to_lost_ratio > 0.15f) {
				setWindowSize(m_window_size - 100);
			} else if (successful_to_lost_ratio > 0.1f) {
				setWindowSize(m_window_size - 50);
			}
		}
	}

	if (bpm_counter > 10.0f) {
		{
			MutexAutoLock internal(m_internal_mutex);
			cur_kbps                 =
					(((float) current_bytes_transfered)/bpm_counter)/1024.0f;
			current_bytes_transfered = 0;
			cur_kbps_lost            =
					(((float) current_bytes_lost)/bpm_counter)/1024.0f;
			current_bytes_lost       = 0;
			cur_incoming_kbps        =
					(((float) current_bytes_received)/bpm_counter)/1024.0f;
			current_bytes_received   = 0;
			bpm_counter              = 0.0f;
		}

		if (cur_kbps > max_kbps) {
			max_kbps = cur_kbps;
		}

		if (cur_kbps_lost > max_kbps_lost) {
			max_kbps_lost = cur_kbps_lost;
		}

		if (cur_incoming_kbps > max_incoming_kbps) {
			max_incoming_kbps = cur_incoming_kbps;
		}

		rate_samples       = MYMIN(rate_samples+1,10);
		float old_fraction = ((float) (rate_samples-1) )/( (float) rate_samples);
		avg_kbps           = avg_kbps * old_fraction +
				cur_kbps * (1.0 - old_fraction);
		avg_kbps_lost      = avg_kbps_lost * old_fraction +
				cur_kbps_lost * (1.0 - old_fraction);
		avg_incoming_kbps  = avg_incoming_kbps * old_fraction +
				cur_incoming_kbps * (1.0 - old_fraction);
	}
}

} // namespace con
