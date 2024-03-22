/// \file
///
/// This file is part of RakNet Copyright 2003 Jenkins Software LLC
///
/// Usage of RakNet is subject to the appropriate license agreement.


#include "RakNetStatistics.h"
#include <stdio.h> // sprintf
#include "GetTime.h"
#include "RakString.h"

using namespace RakNet;

// Verbosity level currently supports 0 (low), 1 (medium), 2 (high)
// Buffer must be hold enough to hold the output string.  See the source to get an idea of how many bytes will be output
void RAK_DLL_EXPORT RakNet::StatisticsToString(const RakNetStatistics *s, char *buffer, int verbosityLevel )
{
	if ( s == 0 )
	{
		sprintf( buffer, "stats is a NULL pointer in statsToString\n" );
		return ;
	}

	if (verbosityLevel==0)
	{
		sprintf(buffer,
			"Bytes per second sent     %llu\n"
			"Bytes per second received %llu\n"
			"Current packetloss        %.1f%%\n",
			(long long unsigned int) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(long long unsigned int) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			s->packetlossLastSecond*100.0f
			);
	}
	else if (verbosityLevel==1)
	{
		sprintf(buffer,
			"Actual bytes per second sent       %llu\n"
			"Actual bytes per second received   %llu\n"
			"Message bytes per second pushed    %llu\n"
			"Total actual bytes sent            %llu\n"
			"Total actual bytes received        %llu\n"
			"Total message bytes pushed         %llu\n"
			"Current packetloss                 %.1f%%\n"
			"Average packetloss                 %.1f%%\n"
			"Elapsed connection time in seconds %llu\n",
			(long long unsigned int) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(long long unsigned int) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			(long long unsigned int) s->valueOverLastSecond[USER_MESSAGE_BYTES_PUSHED],
			(long long unsigned int) s->runningTotal[ACTUAL_BYTES_SENT],
			(long long unsigned int) s->runningTotal[ACTUAL_BYTES_RECEIVED],
			(long long unsigned int) s->runningTotal[USER_MESSAGE_BYTES_PUSHED],
			s->packetlossLastSecond*100.0f,
			s->packetlossTotal*100.0f,
			(long long unsigned int) (uint64_t)((RakNet::GetTimeUS()-s->connectionStartTime)/1000000)
			);

		if (s->BPSLimitByCongestionControl!=0)
		{
			char buff2[128];
			sprintf(buff2,
				"Send capacity                    %llu bytes per second (%.0f%%)\n",
				(long long unsigned int) s->BPSLimitByCongestionControl,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByCongestionControl
				);
			strcat(buffer,buff2);
		}
		if (s->BPSLimitByOutgoingBandwidthLimit!=0)
		{
			char buff2[128];
			sprintf(buff2,
				"Send limit                       %llu (%.0f%%)\n",
				(long long unsigned int) s->BPSLimitByOutgoingBandwidthLimit,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByOutgoingBandwidthLimit
				);
			strcat(buffer,buff2);
		}
	}	
	else
	{
		sprintf(buffer,
			"Actual bytes per second sent         %llu\n"
			"Actual bytes per second received     %llu\n"
			"Message bytes per second sent        %llu\n"
			"Message bytes per second resent      %llu\n"
			"Message bytes per second pushed      %llu\n"
			"Message bytes per second processed   %llu\n"
			"Message bytes per second ignored     %llu\n"
			"Total bytes sent                     %llu\n"
			"Total bytes received                 %llu\n"
			"Total message bytes sent             %llu\n"
			"Total message bytes resent           %llu\n"
			"Total message bytes pushed           %llu\n"
			"Total message bytes received         %llu\n"
			"Total message bytes ignored          %llu\n"
			"Messages in send buffer, by priority %i,%i,%i,%i\n"
			"Bytes in send buffer, by priority    %i,%i,%i,%i\n"
			"Messages in resend buffer            %i\n"
			"Bytes in resend buffer               %llu\n"
			"Current packetloss                   %.1f%%\n"
			"Average packetloss                   %.1f%%\n"
			"Elapsed connection time in seconds   %llu\n",
			(long long unsigned int) s->valueOverLastSecond[ACTUAL_BYTES_SENT],
			(long long unsigned int) s->valueOverLastSecond[ACTUAL_BYTES_RECEIVED],
			(long long unsigned int) s->valueOverLastSecond[USER_MESSAGE_BYTES_SENT],
			(long long unsigned int) s->valueOverLastSecond[USER_MESSAGE_BYTES_RESENT],
			(long long unsigned int) s->valueOverLastSecond[USER_MESSAGE_BYTES_PUSHED],
			(long long unsigned int) s->valueOverLastSecond[USER_MESSAGE_BYTES_RECEIVED_PROCESSED],
			(long long unsigned int) s->valueOverLastSecond[USER_MESSAGE_BYTES_RECEIVED_IGNORED],
			(long long unsigned int) s->runningTotal[ACTUAL_BYTES_SENT],
			(long long unsigned int) s->runningTotal[ACTUAL_BYTES_RECEIVED],
			(long long unsigned int) s->runningTotal[USER_MESSAGE_BYTES_SENT],
			(long long unsigned int) s->runningTotal[USER_MESSAGE_BYTES_RESENT],
			(long long unsigned int) s->runningTotal[USER_MESSAGE_BYTES_PUSHED],
			(long long unsigned int) s->runningTotal[USER_MESSAGE_BYTES_RECEIVED_PROCESSED],
			(long long unsigned int) s->runningTotal[USER_MESSAGE_BYTES_RECEIVED_IGNORED],
			s->messageInSendBuffer[IMMEDIATE_PRIORITY],s->messageInSendBuffer[HIGH_PRIORITY],s->messageInSendBuffer[MEDIUM_PRIORITY],s->messageInSendBuffer[LOW_PRIORITY],
			(unsigned int) s->bytesInSendBuffer[IMMEDIATE_PRIORITY],(unsigned int) s->bytesInSendBuffer[HIGH_PRIORITY],(unsigned int) s->bytesInSendBuffer[MEDIUM_PRIORITY],(unsigned int) s->bytesInSendBuffer[LOW_PRIORITY],
			s->messagesInResendBuffer,
			(long long unsigned int) s->bytesInResendBuffer,
			s->packetlossLastSecond*100.0f,
			s->packetlossTotal*100.0f,
			(long long unsigned int) (uint64_t)((RakNet::GetTimeUS()-s->connectionStartTime)/1000000)
			);

		if (s->BPSLimitByCongestionControl!=0)
		{
			char buff2[128];
			sprintf(buff2,
				"Send capacity                    %llu bytes per second (%.0f%%)\n",
				(long long unsigned int) s->BPSLimitByCongestionControl,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByCongestionControl
				);
			strcat(buffer,buff2);
		}
		if (s->BPSLimitByOutgoingBandwidthLimit!=0)
		{
			char buff2[128];
			sprintf(buff2,
				"Send limit                       %llu (%.0f%%)\n",
				(long long unsigned int) s->BPSLimitByOutgoingBandwidthLimit,
				100.0f * s->valueOverLastSecond[ACTUAL_BYTES_SENT] / s->BPSLimitByOutgoingBandwidthLimit
				);
			strcat(buffer,buff2);
		}
	}
}
