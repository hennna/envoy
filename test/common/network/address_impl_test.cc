#include "envoy/common/exception.h"

#include "common/network/address_impl.h"
#include "test/test_common/network_utility.h"
#include "test/test_common/utility.h"

#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

namespace Network {
namespace Address {
namespace {
bool addressesEqual(const InstancePtr& a, const Instance& b) {
  if (a == nullptr || a->type() != Type::Ip || b.type() != Type::Ip) {
    return false;
  } else {
    return a->ip()->addressAsString() == b.ip()->addressAsString();
  }
}

void makeFdBlocking(int fd) {
  const int flags = ::fcntl(fd, F_GETFL, 0);
  ASSERT_GE(flags, 0);
  ASSERT_EQ(::fcntl(fd, F_SETFL, flags & (~O_NONBLOCK)), 0);
}

void testSocketBindAndConnect(const Instance& loopbackPort) {
  // Create a socket on which we'll listen for connections from clients.
  const int listen_fd = loopbackPort.socket(SocketType::Stream);
  ASSERT_GE(listen_fd, 0) << loopbackPort.asString();
  ScopedFdCloser closer1(listen_fd);

  // Bind the socket to the desired address and port.
  int rc = loopbackPort.bind(listen_fd);
  int err = errno;
  ASSERT_EQ(rc, 0) << loopbackPort.asString() << "\nerror: " << strerror(err) << "\nerrno: " << err;

  // Do a bare listen syscall. Not bothering to accept connections as that would
  // require another thread.
  ASSERT_EQ(::listen(listen_fd, 1), 0);

  // Create a client socket and connect to the server.
  const int client_fd = loopbackPort.socket(SocketType::Stream);
  ASSERT_GE(client_fd, 0) << loopbackPort.asString();
  ScopedFdCloser closer2(client_fd);

  // Instance::socket creates a non-blocking socket, which that extends all the way to the
  // operation of ::connect(), so connect returns with errno==EWOULDBLOCK before the tcp
  // handshake can complete. For testing convenience, re-enable blocking on the socket
  // so that connect will wait for the handshake to complete.
  makeFdBlocking(client_fd);

  // Connect to the server.
  rc = loopbackPort.connect(client_fd);
  err = errno;
  ASSERT_EQ(rc, 0) << loopbackPort.asString() << "\nerror: " << strerror(err) << "\nerrno: " << err;
}
} // namespace

TEST(Ipv4InstanceTest, SocketAddress) {
  sockaddr_in addr4;
  addr4.sin_family = AF_INET;
  EXPECT_EQ(1, inet_pton(AF_INET, "1.2.3.4", &addr4.sin_addr));
  addr4.sin_port = htons(6502);

  Ipv4Instance address(&addr4);
  EXPECT_EQ("1.2.3.4:6502", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("1.2.3.4", address.ip()->addressAsString());
  EXPECT_EQ(6502U, address.ip()->port());
  EXPECT_EQ(IpVersion::v4, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("1.2.3.4"), address));
}

TEST(Ipv4InstanceTest, AddressOnly) {
  Ipv4Instance address("3.4.5.6");
  EXPECT_EQ("3.4.5.6:0", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("3.4.5.6", address.ip()->addressAsString());
  EXPECT_EQ(0U, address.ip()->port());
  EXPECT_EQ(IpVersion::v4, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("3.4.5.6"), address));
}

TEST(Ipv4InstanceTest, AddressAndPort) {
  Ipv4Instance address("127.0.0.1", 80);
  EXPECT_EQ("127.0.0.1:80", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("127.0.0.1", address.ip()->addressAsString());
  EXPECT_FALSE(address.ip()->isAnyAddress());
  EXPECT_EQ(80U, address.ip()->port());
  EXPECT_EQ(IpVersion::v4, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("127.0.0.1"), address));
}

TEST(Ipv4InstanceTest, PortOnly) {
  Ipv4Instance address(443);
  EXPECT_EQ("0.0.0.0:443", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("0.0.0.0", address.ip()->addressAsString());
  EXPECT_TRUE(address.ip()->isAnyAddress());
  EXPECT_EQ(443U, address.ip()->port());
  EXPECT_EQ(IpVersion::v4, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("0.0.0.0"), address));
}

TEST(Ipv4InstanceTest, BadAddress) {
  EXPECT_THROW(Ipv4Instance("foo"), EnvoyException);
  EXPECT_THROW(Ipv4Instance("bar", 1), EnvoyException);
  EXPECT_EQ(parseInternetAddress(""), nullptr);
  EXPECT_EQ(parseInternetAddress("1.2.3"), nullptr);
  EXPECT_EQ(parseInternetAddress("1.2.3.4.5"), nullptr);
  EXPECT_EQ(parseInternetAddress("1.2.3.256"), nullptr);
  EXPECT_EQ(parseInternetAddress("foo"), nullptr);
}

TEST(Ipv4InstanceTest, SocketBindAndConnect) {
  // Test listening on and connecting to an unused port on the IPv4 loopback address.
  Ipv4Instance loopbackPort("127.0.0.1", ::Network::Test::getUnusedPort());
  testSocketBindAndConnect(loopbackPort);
}

TEST(Ipv6InstanceTest, SocketAddress) {
  sockaddr_in6 addr6;
  addr6.sin6_family = AF_INET6;
  EXPECT_EQ(1, inet_pton(AF_INET6, "01:023::00Ef", &addr6.sin6_addr));
  addr6.sin6_port = htons(32000);

  Ipv6Instance address(addr6);
  EXPECT_EQ("[1:23::ef]:32000", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("1:23::ef", address.ip()->addressAsString());
  EXPECT_FALSE(address.ip()->isAnyAddress());
  EXPECT_EQ(32000U, address.ip()->port());
  EXPECT_EQ(IpVersion::v6, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("1:0023::0Ef"), address));
}

TEST(Ipv6InstanceTest, AddressOnly) {
  Ipv6Instance address("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  EXPECT_EQ("[2001:db8:85a3::8a2e:370:7334]:0", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("2001:db8:85a3::8a2e:370:7334", address.ip()->addressAsString());
  EXPECT_EQ(0U, address.ip()->port());
  EXPECT_EQ(IpVersion::v6, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("2001:db8:85a3::8a2e:0370:7334"), address));
}

TEST(Ipv6InstanceTest, AddressAndPort) {
  Ipv6Instance address("::0001", 80);
  EXPECT_EQ("[::1]:80", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("::1", address.ip()->addressAsString());
  EXPECT_EQ(80U, address.ip()->port());
  EXPECT_EQ(IpVersion::v6, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("0:0:0:0:0:0:0:1"), address));
}

TEST(Ipv6InstanceTest, PortOnly) {
  Ipv6Instance address(443);
  EXPECT_EQ("[::]:443", address.asString());
  EXPECT_EQ(Type::Ip, address.type());
  EXPECT_EQ("::", address.ip()->addressAsString());
  EXPECT_TRUE(address.ip()->isAnyAddress());
  EXPECT_EQ(443U, address.ip()->port());
  EXPECT_EQ(IpVersion::v6, address.ip()->version());
  EXPECT_TRUE(addressesEqual(parseInternetAddress("::0000"), address));
}

TEST(Ipv6InstanceTest, BadAddress) {
  EXPECT_THROW(Ipv6Instance("foo"), EnvoyException);
  EXPECT_THROW(Ipv6Instance("bar", 1), EnvoyException);
  EXPECT_EQ(parseInternetAddress("0:0:0:0"), nullptr);
  EXPECT_EQ(parseInternetAddress("fffff::"), nullptr);
  EXPECT_EQ(parseInternetAddress("/foo"), nullptr);
}

TEST(Ipv6InstanceTest, SocketBindAndConnect) {
  // Test listening on and connecting to an unused port on the IPv4 loopback address.
  Ipv6Instance loopbackPort("::1", ::Network::Test::getUnusedPort());
  testSocketBindAndConnect(loopbackPort);
}

TEST(PipeInstanceTest, Basic) {
  PipeInstance address("/foo");
  EXPECT_EQ("/foo", address.asString());
  EXPECT_EQ(Type::Pipe, address.type());
  EXPECT_EQ(nullptr, address.ip());
}

} // Address
} // Network
