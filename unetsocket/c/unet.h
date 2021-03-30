#ifndef _UNET_H_
#define _UNET_H_

#include "fjage.h"

typedef void *unetsocket_t;        ///< unet socket connection

/// Maximum length of a frame ID string

#define FRAME_ID_LEN        64

/// Transmit sampling rate

#define TXSAMPLINGFREQ           192000 //Hz

/// Timeout

#define TIMEOUT                  1000   //ms

/// Well-known protocol number assignments.

#define	DATA                     0      // Protocol number for user application data.
#define RANGING				     1      // Protocol number for use by ranging agents.
#define LINK 					 2      // Protocol number for use by link agents.
#define REMOTE                   3      // Protocol number for use by remote management agents.
#define MAC 					 4      // Protocol number for use by MAC protocol agents.
#define ROUTING 				 5      // Protocol number for use by routing agents.
#define TRANSPORT 				 6      // Protocol number for use by transport agents.
#define ROUTE_MAINTENANCE 		 7 		// Protocol number for use by route maintenance agents.
#define LINK2 					 8      // Protocol number for use by secondary link agents.
#define USER 					32      // Lowest protocol number allowable for user protocols.
#define MAX 					63      // Largest protocol number allowable.

/// Open a unet socket connection to the modem.
///
/// @param hostname         Host name or IP address
/// @param port             Port number
/// @return                 Unet socket

unetsocket_t unetsocket_open(const char* hostname, int port);

#ifndef _WIN32
/// Open a unet socket connection to the modem.
///
/// @param devname          Device name
/// @param baud             Baud rate
/// @param settings         RS232 settings (NULL or "N81")
/// @return                 Unet socket

unetsocket_t unetsocket_rs232_open(const char* devname, int baud, const char* settings);
#endif

/// Close connection to the modem.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_close(unetsocket_t sock);

/// Checks if a socket is closed.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_is_closed(unetsocket_t sock);

/// Binds a socket to listen to a specific protocol datagrams.
/// Protocol numbers between Protocol.DATA+1 to Protocol.USER-1 are reserved
/// protocols and cannot be bound. Unbound sockets listen to all unreserved
/// protocols.
///
/// @param sock             Unet socket
/// @param protocol         Protocol number to listen for
/// @return                 0 on success, -1 otherwise

int unetsocket_bind(unetsocket_t sock, int protocol);

/// Unbinds a socket so that it listens to all unreserved protocols.
/// Protocol numbers between Protocol.DATA+1 to Protocol.USER-1 are considered
/// reserved.
///
/// @param sock             Unet socket

void unetsocket_unbind(unetsocket_t sock);

/// Checks if a socket is bound.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_is_bound(unetsocket_t sock);

/// Sets the default destination address and destination protocol number for
/// datagrams sent using this socket. The defaults can be overridden for specific
/// send() calls.
/// The default protcol number when a socket is opened is Protcol.DATA. The default
/// node address is undefined. Protocol numbers between Protocol.DATA+1 to
/// Protocol.USER-1 are considered reserved, and cannot be used for sending datagrams
/// using the socket.
///
/// @param sock             Unet socket
/// @param to               Default destination node address
/// @param protocol         Default protocol number
/// @return                 0 on success, -1 otherwise

int unetsocket_connect(unetsocket_t sock, int to, int protocol);

/// Resets the default destination address to undefined, and the default protocol
/// number to Protocol.DATA.
///
/// @param sock             Unet socket

void unetsocket_disconnect(unetsocket_t sock);

/// Checks if a socket is connected, i.e., has a default destination address and
/// protocol number.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_is_connected(unetsocket_t sock);

/// Gets the local node address.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_get_local_address(unetsocket_t sock);

/// Gets the protocol number that the socket is bound to.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_get_local_protocol(unetsocket_t sock);

/// Gets the default destination node address for a connected socket.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_get_remote_address(unetsocket_t sock);

/// Gets the default transmission protocol number.
///
/// @param sock             Unet socket
/// @return                 0 on success, -1 otherwise

int unetsocket_get_remote_protocol(unetsocket_t sock);

/// Sets the timeout for datagram reception. The default timeout is infinite,
/// i.e., the :func:`~receive()` call blocks forever. A timeout of 0 means the
/// :func:`~receive()` call is non-blocking.
///
/// @param sock             Unet socket
/// @param ms               Timeout in milliseconds, or -1 for infinite timeout

void unetsocket_set_timeout(unetsocket_t sock, long ms);

/// Gets the timeout for datagram reception.
///
/// @param sock             Unet socket
/// @return                 Timeout in milliseconds, 0 for non-blocking, or -1 for infinite

long unetsocket_get_timeout(unetsocket_t sock);

/// Transmits a datagram to the specified node address using the specified protocol.
/// Protocol numbers between Protocol.DATA+1 to Protocol.USER-1 are considered reserved,
/// and cannot be used for sending datagrams using the socket.
///
/// @param sock             Unet socket
/// @param data             Data to send across
/// @param len              Number of bytes in the data
/// @param to               Destination node address
/// @param protocol         Protocol number
/// @return                 0 on success, -1 otherwise

int unetsocket_send(unetsocket_t sock, uint8_t* data, int len, int to, int protocol);

/// Transmits a datagram to the specified node address using the specified protocol.
/// Protocol numbers between Protocol.DATA+1 to Protocol.USER-1 are considered reserved,
/// and cannot be used for sending datagrams using the socket.
///
/// @param sock             Unet socket
/// @param req              Datagram transmission request
/// @return                 0 on success, -1 otherwise

int unetsocket_send_request(unetsocket_t sock, fjage_msg_t req); // req must be a DatagramReq

/// Receives a datagram sent to the local node and the bound protocol number. If the
/// socket is unbound, then datagrams with all unreserved protocols are received. Any
/// broadcast datagrams are also received.
/// This call blocks until a datagram is available, the socket timeout is reached,
/// or until :func:`~unetsocket_cancel()` is called.
///
/// @param sock             Unet socket
/// @return                 Datagram reception notification, or NULL if none available

fjage_msg_t unetsocket_receive(unetsocket_t sock); // returns a DatagramNtf

/// Get the range information
///
/// @param sock             Unet socket
/// @param to               Address of the node to which range is requested
/// @param range            Measured range

int unetsocket_get_range(unetsocket_t sock, int to, float* range);

/// Set the transmission power level of frame
///
/// @param sock             Unet socket
/// @param index            Index of the modulation scheme (1 for CONTROL scheme and 2 for DATA scheme)
/// @param value            Transmission power level

int unetsocket_set_powerlevel(unetsocket_t sock, int index, float value);

/// Transmit a signal.
///
/// For a 18-36 KHz modem, set fc to 24000 for baseband signal transmission
/// TODO: Add recommendations for LF, HF modems
///
/// @param modem            Gateway
/// @param signal           Baseband signal or Passband signal.
///                         Baseband signal must be an sequence of
///                         numbers with alternating real and imaginary values.
///                         Passband signal is a real time series to transmit.
/// @param nsamples         Number of samples.
///                         For baseband signal, this is equal to number of
///                         baseband samples.
/// @param rate             Baseband/ Passband sampling rate of signal in Hz
/// @param fc               Signal carrier frequency in Hz for passband transmission.
/// @return                 0 on success, -1 otherwise

int unetsocket_tx_signal(unetsocket_t sock, float *signal, int nsamples, int rate, float fc, char *id);

/// Record a baseband signal.
///
/// @param modem            Gateway
/// @param buf              Buffer to store the recorded signal
/// @param nsamples         Number of samples
/// @return                 0 on success, -1 otherwise

int unetsocket_record(unetsocket_t sock, float *buf, int nsamples);

/// Cancels an ongoing blocking receive().
///
/// @param sock             Unet socket

void unetsocket_cancel(unetsocket_t sock);

/// Gets a Gateway to provide low-level access to UnetStack.
///
/// @param sock             Unet socket
/// @return 				Gateway

fjage_gw_t unetsocket_get_gateway(unetsocket_t sock);

/// Gets an AgentID providing a specified service for low-level access to UnetStack.
///
/// @param sock             Unet socket
/// @param svc              Fully qualified name of a service
/// @return                 AgentID of an agent providing the service, NULL if none found

fjage_aid_t unetsocket_agent_for_service(unetsocket_t sock, const char* svc);

/// Gets a list of AgentIDs providing a specified service for low-level access to UnetStack.
///
/// @param sock             Unet socket
/// @param svc              Fully qualified name of a service
/// @param agents           Pointer to array containing list of AgentIDs found
/// @param max              Maximum number of elements allowed in the list
/// @return                 0 on success, -1 otherwise

int unetsocket_agents_for_service(unetsocket_t sock, const char* svc, fjage_aid_t* agents, int max);

/// Gets a named AgentID for low-level access to UnetStack. The AgentID created using this function should be freed using
/// fjage_aid_destroy().
///
/// @param name             Name of the agent
/// @return                 AgentID of an agent providing the service, NULL if none found

fjage_aid_t unetsocket_agent(const char* name);

/// Resolve node name to node address.
///
/// @param sock             Unet socket
/// @param node_name        Name of node to resolve
/// @return                 0 on success, -1 otherwise

int unetsocket_host(unetsocket_t sock, const char* node_name);

#endif

