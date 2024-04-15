class Async::IO
  case Async::BACKEND
  when :io_uring
  when :poll
  when :select
  end
end
