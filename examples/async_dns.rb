poll = Poll.new
server = TCPServer.new('::', 0)
ares = Async::Ares.new
ares.getaddrinfo(server, "www.ruby-lang.org", "https")
ares.getsock do |socket|
  poll.add(socket.socket, (socket.readable? ? Poll::In : 0) | (socket.writable? ? Poll::Out : 0))
end
while (timeout = ares.timeout)
  res = poll.wait(timeout * 1000.0) do |pfd|
    ares.process_fd((pfd.readable?) ? pfd.socket : -1, (pfd.writable?) ? pfd.socket : -1)
  end
  poll.clear
  ares.getsock do |socket|
    poll.add(socket.socket, (socket.readable? ? Poll::In : 0) | (socket.writable? ? Poll::Out : 0))
  end
  unless res
    puts res.inspect
  end
end
puts ares.cnames.inspect
puts ares.ais.inspect
puts ares.errors.inspect
