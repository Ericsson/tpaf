#!/usr/bin/python3

#
# check-release.py -- script to verify tpaf releases before pushing
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Ericsson AB
#

import getopt
import git
import os
import re
import subprocess
import sys
import tempfile
import time

from functools import total_ordering


def usage(name):
    print("%s [-c <cmd>] <release-sha|release-tag>" % name)
    print("Options:")
    print("  -c <cmd>  Run command <cmd>. Default is to run all.")
    print("  -m        Enable valgrind in the test suite.")
    print("  -h        Print this text.")
    print("Commands:")
    print("  meta     Only check release meta data.")
    print("  changes  Only list changes with previous release.")
    print("  test     Only run the test suites.")


def prefix(msg, *args):
    print("%s: " % msg, end="")
    print(*args, end="")
    print(".")


def fail(*args):
    prefix("ERROR", *args)
    sys.exit(1)


def note(*args):
    prefix("NOTE", *args)


@total_ordering
class Version:
    def __init__(self, major, minor, patch=None):
        self.major = major
        self.minor = minor
        self.patch = patch

    def __str__(self):
        s = "%d.%d" % (self.major, self.minor)
        if self.patch is not None:
            s += ".%d" % self.patch
        return s

    def __lt__(self, other):
        if self.major == other.major:
            if self.minor == other.minor:
                if self.patch is None:
                    assert other.patch is None
                    return False
                return self.patch < other.patch
            return self.minor < other.minor
        else:
            return self.major < other.major

    def __eq__(self, other):
        if self.major != other.major or  \
           self.minor != other.minor:
            return False
        if self.patch is None:
            return other.patch is None
        else:
            if other.patch is None:
                return False
            return self.patch == other.patch


def get_release_tags(repo):
    return [t for t in repo.tags if tag_re.match(t.name)]


def get_commit_release_tags(repo, commit):
    return [tag for tag in get_release_tags(repo) if tag.commit == commit]


def get_commit_release_tag(repo, commit):
    tags = get_commit_release_tags(repo, commit)

    if len(tags) != 1:
        fail("Could not find exactly one release tag for commit %s" %
             release_commit)

    return tags[0]


def get_release_versions(repo):
    return [get_tag_version(tag) for tag in get_release_tags(repo)]


def get_prev_release_tag(repo, release_version):

    release_tags = get_release_tags(repo)

    candidate = None

    for tag in release_tags[1:]:
        v = get_tag_version(tag)

        if v.major != release_version.major or \
           v.minor > release_version.minor:
            continue

        if v.minor == release_version.minor and \
           v.patch >= release_version.patch:
            continue

        if candidate is None:
            candidate = tag
            continue

        candidate_v = get_tag_version(candidate)
        if v.minor > candidate_v.minor:
            candidate = tag
            continue

        if v.minor == candidate_v.minor and v.patch > candidate_v.patch:
            candidate = tag
            continue

    return candidate


tag_re = re.compile('^v[0-9]+')


def tag_name(impl_version):
    return "v%s" % impl_version


def get_tag_version(tag):
    assert tag.name[0] == 'v'
    v = tag.name[1:].split('.')
    major = int(v[0])
    minor = int(v[1])
    patch = int(v[2])
    return Version(major, minor, patch)


hdr_version_re = re.compile(r'TPAF_VERSION "([0-9]+)\.([0-9]+)\.([0-9]+)"')


def get_hdr_version(release_commit, release_tag):
    with tempfile.TemporaryDirectory() as temp_dir:
        cmd = build_cmd("", EXTRA_BUILD_CFLAGS, release_commit, release_tag,
                        temp_dir)
        run(cmd)

        tpaf_dir = "tpaf-%s" % get_tag_version(release_tag)

        hdr_filename = os.path.join(temp_dir, tpaf_dir,
                                    "src/daemon/tpaf_version.h")
        hdr = open(hdr_filename).read()
        m = hdr_version_re.search(hdr)

        major = int(m.groups()[0])
        minor = int(m.groups()[1])
        if len(m.groups()) == 3:
            patch = int(m.groups()[2])
        else:
            patch = None
        return Version(major, minor, patch)


def check_meta(repo, release_commit):
    release_tag = get_commit_release_tag(repo, release_commit)

    hdr_version = get_hdr_version(release_commit, release_tag)
    tag_version = get_tag_version(release_tag)

    if hdr_version != tag_version:
        fail("Header has version %s and tag suggests %s" %
             (hdr_version, tag_version))

    prev_release_tag = get_prev_release_tag(repo, tag_version)
    if prev_release_tag is None:
        note("Unable to find the release previous to %s" % tag_version)
    else:
        prev_hdr_version = get_hdr_version(prev_release_tag.commit,
                                           prev_release_tag)
        prev_reg_version = get_tag_version(prev_release_tag)

    print("Release information:")
    print("  Header version (from \"tpaf_version.h\"): %s" % hdr_version)
    print("  Git tag version: %s" % tag_version)
    print("  Commit:")
    print("    SHA: %s" % release_commit.hexsha)
    print("    Summary: %s" % release_commit.summary)

    if prev_release_tag is not None:
        print("  Previous release: %s (tag %s)" % (prev_hdr_version,
                                                   prev_tag_version))
    print("Releases:")
    for version in get_release_versions(repo):
        print("  %s" % version)


def check_changes(repo, release_commit):
    release_tag = get_commit_release_tag(repo, release_commit)

    prev_release_tag = get_prev_release_tag(repo, get_tag_version(release_tag))

    if prev_release_tag is not None:
        rev = '%s..%s' % (prev_release_tag, release_tag)
        print("Changes between %s and %s:" % (get_tag_version(prev_release_tag),
                                              get_tag_version(release_tag)))
    else:
        print("Changes in the first release:")
        rev = '%s' % release_tag

    for commit in repo.iter_commits(rev=rev):
        short_sha = repo.git.rev_parse(commit, short=True)
        print(" %s %s" % (short_sha, commit.summary))


def run(cmd, exit_on_error=True):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, encoding='utf-8')

    if res.returncode != 0:
        sys.stderr.write(res.stdout)
        if exit_on_error:
            sys.exit(1)
        else:
            return True
    return False


def assure_sudo():
    subprocess.run("sudo echo -n", shell=True, check=True)


def test_build_separate_build_dir(repo, release_commit):
    release_tag = get_commit_release_tag(repo, release_commit)

    print("Test build w/ separate build directory.")
    cmd = """
set -e
tmpdir=`mktemp -d`; \\
tpafdir=tpaf-%s; \\
tarfile=$tmpdir/$tpafdir.tar; \\
git archive --prefix=$tpafdir/ --format=tar -o $tarfile %s; \\
cd $tmpdir; \\
tar xf $tarfile; \\
cd $tpafdir; \\
autoreconf -i; \\
mkdir build; \\
cd build; \\
../configure; \\
make -j; \\
""" % (tag_version(release_tag), release_commit)

    run(cmd)


EXTRA_BUILD_CFLAGS="-Werror"

# to make more old releases build
EXTRA_ABI_CFLAGS="-Wno-error"

def build_cmd(conf, cflags, release_commit, release_tag, build_dir):
    env_cflags = os.environ.get('CFLAGS')
    if env_cflags is not None:
        cflags += (" %s" % env_cflags)

    conf += (" CFLAGS=\"%s\"" % cflags)

    return """
set -e
tmpdir=%s; \\
tpafdir=tpaf-%s; \\
tarfile=$tmpdir/$tpafdir.tar; \\
git archive --prefix=$tpafdir/ --format=tar -o $tarfile %s;\\
cd $tmpdir; \\
tar xf $tarfile; \\
cd $tpafdir; \\
autoreconf -i; \\
./configure %s; \\
make -j; \\
""" % (build_dir, get_tag_version(release_tag), release_commit, conf)


def run_test(repo, conf, release_commit, use_valgrind):
    release_tag = get_commit_release_tag(repo, release_commit)

    if use_valgrind:
        conf += " --enable-valgrind"

    print("Running test ", end="")
    if conf == "":
        print("using default configure options.")
    else:
        print("using configure options: \"%s\"." % conf)

    temp_dir = tempfile.TemporaryDirectory()

    cmd = build_cmd(conf, EXTRA_BUILD_CFLAGS, release_commit, release_tag,
                    temp_dir.name)

    cmd += """
make check; \\
sudo make check \\
"""

    run(cmd)

    temp_dir.cleanup()

def run_tests(repo, release_commit, use_valgrind):
    assure_sudo()

    test_build_separate_build_dir(repo, release_commit)

    run_test(repo, "", release_commit, use_valgrind)


def check_repo(repo):
    if repo.is_dirty():
        fail("Repository contains modifications.")


optlist, args = getopt.getopt(sys.argv[1:], 'c:mh')

cmd = None
use_valgrind = False

for opt, optval in optlist:
    if opt == '-h':
        usage(sys.argv[0])
        sys.exit(0)
    if opt == '-c':
        cmd = optval
    if opt == '-m':
        use_valgrind = True

if len(args) != 1:
    usage(sys.argv[0])
    sys.exit(1)

repo = git.Repo()
check_repo(repo)

release_commit = repo.commit(args[0])

meta = False
changes = False
test = False

if cmd == 'meta':
    meta = True
elif cmd == 'changes':
    changes = True
elif cmd == 'test':
    test = True
elif cmd is None:
    meta = True
    changes = True
    abi = True
    test = True
else:
    print("Unknown cmd '%s'." % cmd)
    sys.exit(1)

if meta:
    check_meta(repo, release_commit)
if changes:
    check_changes(repo, release_commit)
if test:
    run_tests(repo, release_commit, use_valgrind)
