source "https://rubygems.org"
# Hello! This is where you manage which Jekyll version is used to run.
# When you want to use a different version, change it below, save the
# file and run `bundle install`. Run Jekyll with `bundle exec`, like so:
#
#     bundle exec jekyll serve
#
# This will help ensure the proper Jekyll version is running.
# Happy Jekylling!

gem "github-pages", "~> 231", group: :jekyll_plugins
# github-pages v231 requires Ruby v3.0+ which doesn't come with webricks
# Jekyll v4.3+ hard requires webricks but apparently not Jekyll v3.9
# What a mess
gem "webrick", "~> 1.8"

# If you have any plugins, put them here!
group :jekyll_plugins do
  gem "jekyll-compose", "~> 0.12"
end

# Windows and JRuby does not include zoneinfo files, so bundle the tzinfo-data gem
# and associated library.
platforms :mingw, :x64_mingw, :mswin, :jruby do
  gem "tzinfo", "~> 1.2"
  gem "tzinfo-data"
end

# Performance-booster for watching directories on Windows
gem "wdm", "~> 0.1.1", :platforms => [:mingw, :x64_mingw, :mswin]

# Lock `http_parser.rb` gem to `v0.6.x` on JRuby builds since newer versions of the gem
# do not have a Java counterpart.
gem "http_parser.rb", "~> 0.6.0", :platforms => [:jruby]

# Dependencies for docs
gem "asciidoctor", "~> 2.0"
gem "rouge", "~> 3.30"