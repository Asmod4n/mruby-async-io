MRuby::Gem::Specification.new('mruby-async-io') do |spec|
  unless spec.search_package('libcares')
    raise "mruby-async-io: can't find c-ares libraries or development headers, please install them."
  end

  spec.license = 'Apache-2.0'
  spec.author  = 'Hendrik Beskow'
  spec.summary = 'Async::IO for mruby'
  spec.add_dependency 'mruby-socket'
  spec.add_dependency 'mruby-chrono'
  spec.add_dependency 'mruby-poll'

  if spec.search_package('liburing')
    spec.cc.defines << 'HAVE_IO_URING_H'
    spec.add_dependency 'mruby-io-uring'
  elsif spec.search_header_path('poll.h')
    spec.cc.defines << 'HAVE_POLL_H'
    spec.add_dependency 'mruby-poll'
  else
    spec.cc.defines << 'HAVE_SELECT_H'
    spec.add_dependency 'mruby-io'
  end
end
