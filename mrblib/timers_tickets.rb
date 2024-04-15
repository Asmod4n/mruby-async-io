class Async::IO::Timers_Tickets
  attr_reader :ticket_delay

  def initialize(ticket_delay)
    @timers = []
    @tickets = []
    @ticket_delay = ticket_delay
  end

  def timer(delay, times = nil, &block)
    timer = Timer.new(self, delay, times, &block)
    @timers << timer
    @timers.sort!
    timer
  end

  def timers_sort
    @timers.sort!
  end

  def timer_remove(timer)
    @timers.delete(timer)
    self
  end

  def ticket(&block)
    ticket = Ticket.new(self, &block)
    @tickets << ticket
    ticket
  end

  def ticket_reset(ticket)
    ticket = @tickets.delete_at(@tickets.rindex(ticket))
    @tickets << ticket
    ticket.when = Chrono::Steady.now + @ticket_delay
    self
  end

  def ticket_delete(ticket)
    @tickets.delete_at(@tickets.rindex(ticket))
    self
  end

  def timeout
    wann = Chrono::Steady.now + 1000
    unless @timers.empty?
      wann = @timers.min.when
    end
    if (ticket = @tickets.first)
      wann = ticket.when if wann > ticket.when
    end
    tickless = wann - Chrono::Steady.now
    tickless < 0 ? 0 : tickless
  end

  def execute
    now = Chrono::Steady.now
    @timers.take_while {|timer| now >= timer.when}.each {|timer| timer.call}
    @tickets.take_while {|ticket| now >= ticket.when}.each {|ticket| ticket.call}
    self
  end

  class Timer
    attr_reader :delay, :times, :when

    def initialize(timers_tickets, delay, times = nil, &block)
      @when = Chrono::Steady.now + delay
      @timers_tickets = timers_tickets
      @delay = delay
      @times = times
      @block = block
    end

    def <=>(other)
      @when <=> other.when
    end

    def delay=(delay)
      @when = Chrono::Steady.now + delay
      @delay = delay
    end

    def times=(times)
      @times = times
    end

    def reset
      @when = Chrono::Steady.now + @delay
      @timers_tickets.timers_sort
    end

    def remove
      @timers_tickets.timer_remove(self)
    end

    def call
      @block.call(self)
      if @times && (@times -= 1) <= 0
        @timers_tickets.timer_delete(self)
      else
        @when += @delay
      end
    end
  end

  class Ticket
    attr_accessor :when

    def initialize(timers_tickets, &block)
      @timers_tickets = timers_tickets
      @block = block
      @when = Chrono::Steady.now + timers_tickets.ticket_delay
    end

    def reset
      @timers_tickets.ticket_reset(self)
    end

    def call
      @block.call(self)
      @timers_tickets.ticket_delete(self)
    end
  end
end
