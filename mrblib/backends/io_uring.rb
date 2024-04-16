class Async::IO::IO_Uring < Async::IO::Backend
  def initialize(entries = 2048, flags = 0)
    @uring = ::IO::Uring.new(entries, flags)
    @operations = []
  end

  def do_operation(socket, operation, *options)
    @operations << [Fiber.current, operation]
    case operation
    when :socket, :tcp_socket, :tcp_server
      @uring.prep_socket(*options)
    when :accept
      @uring.prep_accept(socket, *options)
    end
    Fiber.yield
  end

  def run_once(timeout = -1.0)
    @uring.wait(timeout) do |userdata|
      raise userdata.errno if userdata.errno
      fiber, operation = @operations.shift
      case operation
      when :tcp_socket, :accept
        socket = ::TCPSocket.for_fd(userdata.res)
        socket._setnonblock(true)
        fiber.resume(socket)
      when :tcp_server
        server = ::TCPServer.for_fd(userdata.res)
        server._setnonblock(true)
        fiber.resume(server)
      when :socket
        socket = ::BasicSocket.for_fd(userdata.res)
        socket._setnonblock(true)
        fiber.resume(socket)
      when :recv
        fiber.resume(userdata.buf)
      else
        fiber.resume(userdata.res)
      end
    end
  end
end
