uring = IO::Uring.new
server = TCPServer.new('::', 0)
server._setnonblock(true)
ares = Async::Ares.new do |socket|
  uring.prep_poll_add(socket, (socket.readable? ? IO::Uring::POLLIN : 0) | (socket.writable? ? IO::Uring::POLLOUT : 0))
end

ares.getaddrinfo(server, "www.ruby-lang.org", "https")

while (timeout = ares.timeout)
  res = uring.wait(timeout) do |userdata|
    ares.process_fd((userdata.readable?) ? userdata.sock : -1, (userdata.writable?) ? userdata.sock : -1)
  end
end

puts ares.cnames.inspect
puts ares.ais.inspect
puts ares.errors.inspect
