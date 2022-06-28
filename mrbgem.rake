# for MRuby::Source::MRUBY_RELEASE_NO
begin
  require 'mruby/source'
rescue LoadError
  $: << File.join(MRUBY_ROOT, 'lib')
  begin
    require 'mruby/source'
  rescue LoadError
    # for mruby 1.0.0 or 1.1.0
  end
end

MRuby::Gem::Specification.new('mruby-require') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Internet Initiative Japan Inc.'

  spec.add_dependency 'mruby-array-ext', core: 'mruby-array-ext'
  spec.add_dependency 'mruby-compiler', core: 'mruby-compiler'

  if defined?(MRuby::Source::MRUBY_RELEASE_NO) && MRuby::Source::MRUBY_RELEASE_NO >= 10400
    spec.add_dependency 'mruby-io', core: 'mruby-io'
  else
    spec.add_dependency 'mruby-io', github: 'iij/mruby-io'
  end

  spec.add_test_dependency 'mruby-dir', github: 'iij/mruby-dir'
  spec.add_test_dependency 'mruby-tempfile', github: 'iij/mruby-tempfile'
  spec.add_test_dependency 'mruby-time', core: 'mruby-time'
end
