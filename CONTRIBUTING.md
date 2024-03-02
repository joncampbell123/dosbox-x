## Setting up the dev environment (extremely brief version)

This is a [Jekyll](https://jekyllrb.com/) site. If you want to develop it, I recommend to install the site's dependencies with [Bundler](https://bundler.io), assuming that you have Ruby 3.1 installed:
```sh
$ bundle install
```

Then you can utilize the Jekyll CLI to preview your changes to the site locally and live:
```sh
$ bundle exec jekyll serve
```

## Adding Markdown content

If you feel fancy you can use [Pages CMS](https://pagescms.org/) to deal with all this

### Updating the latest version

Before doing this, you probably want to make sure all files for the release are ready and uploaded to GitHub, to make users' life easier

1. Add a release notes page for the latest version, see below
2. Open [`_data/latest_version.yml`](_data/latest_version.yml)
3. Edit the fields accordingly
4. You're basically done

### Adding a release note page

1. Create a file in [`_release_notes/`](_release_notes/) by the name of `<release name>.md` and open it
2. Add a front matter like below to the start of the file, then append a new line after it
    ```markdown
    ---
    slug: "<insert version number>"
    date: "<insert release date of version, in MMMM D, YYYY format>"
    ---
    ```
3. Extract the release notes from `CHANGELOG` or write your own, make sure it's in Markdown format, then append it to the file
4. Optionally, serve the site and tinker with the page until it looks OK
    > The page will be located at `/release-<version name>.html`
5. You're basically done

### Writing a new page

Just do it as one would with a normal Jekyll site (see [this](https://jekyllrb.com/docs/pages/) for pages and [this](https://jekyllrb.com/docs/posts/) for posts), except if you're writing a blog post for some reason - don't set the layout if so. You can [use Jekyll Compose](https://github.com/jekyll/jekyll-compose#usage) for this if you'd like some shortcuts.

## Updating the wiki

> If you mean to contribute to the wiki, I'll direct you to [rderooy/dosbox-x-wiki](https://github.com/rderooy/dosbox-x-wiki) as that's where wiki updates are pulled from right now

First if you haven't yet, make a clone of https://github.com/joncampbell123/dosbox-x.wiki.git outside this repo first

After you have that ready, you can then do something like this in Bash to update the wiki:
```bash
$ cd path/to/dosbox-x.wiki.git
$ git pull

$ cd path/to/dosbox-x-gh-pages
$ ./update-wiki ./wiki path/to/dosbox-x.wiki.git
```