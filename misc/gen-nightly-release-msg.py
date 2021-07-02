import subprocess

with open('misc/NIGHTLY.md') as f:
    template = f.read()

images = ['ricerca-release.iso', 'ricerca-debug.iso',
          'ricerca-safe.iso', 'ricerca-profile.iso']

encoding = 'utf8'


def run(cmd):
    return subprocess.check_output(cmd).decode("utf8")[:-1]


template = template.replace('$VERSION', 'v0.0.0-' +
                            run(['git', 'rev-parse', '--short', 'HEAD']))
template = template.replace('$SHA1_INFO', run(['sha1sum'] + images))
template = template.replace(
    '$SHA256_INFO', run(['sha256sum'] + images))
template = template.replace(
    '$SHA512_INFO', run(['sha512sum'] + images))
template = template.replace(
    '$MD5_INFO', run(['md5sum'] + images))
template = template.replace(
    '$SYSROOT_SHA1_INFO', run(['bash', '-c', 'find build/system-root/ -type f | xargs sha256sum -b']))

with open('misc/NIGHTLY.out.md', 'w') as f:
    f.write(template)
