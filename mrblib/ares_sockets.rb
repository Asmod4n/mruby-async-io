class Async::Ares::Socket
  attr_reader :socket, :readable, :writable

  def initialize(socket, readable, writable)
    @socket, @readable, @writable = socket, readable, writable
  end

  alias_method :fileno, :socket
  alias_method :to_i,   :socket

  alias_method :readable?, :readable
  alias_method :writable?, :writable
end
