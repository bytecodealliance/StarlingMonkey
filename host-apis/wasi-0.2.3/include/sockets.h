#ifndef SOCKETS_H
#define SOCKETS_H

#include "host_api.h"

namespace host_api {


/**
 * \class TCPSocket
 * \brief A bare-bones representation of a TCP socket, supporting only basic, blocking operations.
 *
 * This class provides methods to create, connect, send, and receive data over a TCP socket.
 */
class TCPSocket : public Resource {
protected:
  TCPSocket(std::unique_ptr<HandleState> state);

public:
  using AddressIPV4 = std::tuple<uint8_t, uint8_t, uint8_t, uint8_t>;
  using Port = uint16_t;

  enum IPAddressFamily {
    IPV4,
    IPV6,
  };

  TCPSocket() = delete;

  /**
   * \brief Factory method to create a TCPSocket.
   * \param address_family The IP address family (IPv4 or IPv6).
   * \return A pointer to the created TCPSocket.
   */
  static TCPSocket* make(IPAddressFamily address_family);

  ~TCPSocket() override = default;

  /**
   * \brief Connects the socket to a specified address and port synchronously,
   *        blocking until the connection is established.
   * \param address The IPv4 address to connect to.
   * \param port The port to connect to.
   * \return True if the connection is successful, false otherwise.
   */
  bool connect(AddressIPV4 address, Port port);

  /**
   * \brief Closes the socket if it is open, no-op otherwise.
   */
  void close();

  /**
   * \brief Sends data over the socket synchronously, blocking until the data is sent.
   * \param chunk The data to send.
   * \return True if the data is sent successfully, false otherwise.
   */
  bool send(HostString chunk);

  /**
   * \brief Receives data from the socket.
   * \param chunk_size The size of the data chunk to receive.
   * \return The received data as a HostString.
   */
  HostString receive(uint32_t chunk_size);
};

}

#endif //SOCKETS_H
