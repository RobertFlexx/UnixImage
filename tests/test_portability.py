"""Portable build/authentication regressions; never opens or writes a device.

Run with python3 -m unittest discover -s tests -v. GTK tests need GTK3 development
files. Privilege tools are replaced with local stubs; no real sudo is invoked.
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def run(args, **kwargs):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=60, **kwargs)


def script(path, contents):
    path.write_text('#!/bin/sh\nset -eu\n' + contents)
    path.chmod(0o755)
    return path


class PortabilityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='uniximage-tests-')
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.env = dict(os.environ, TMPDIR=str(self.base))
        self.prefix = self.base / 'prefix with spaces'

    def install(self, **env):
        result = run(['/bin/sh', str(ROOT / 'install.sh')], cwd='/',
                     env=dict(self.env, INSTALL_PREFIX=str(self.prefix), **env))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(list(self.base.glob('uniximage-build-*')))
        return result

    def test_cli_user_install_and_staging(self):
        stage = self.base / 'stage'
        self.install(GUI='no', DESTDIR=str(stage))
        target = Path(str(stage) + str(self.prefix))
        cli = target / 'bin/uniximage-cli'
        self.assertEqual(cli.read_bytes(), (target / 'bin/uniximage').read_bytes())
        self.assertEqual(cli.stat().st_mode & 0o777, 0o755)
        self.assertEqual(run([str(cli), '--version']).returncode, 0)
        desktop = (target / 'share/applications/uniximage.desktop').read_text()
        self.assertIn('Terminal=true', desktop)
        self.assertIn('Exec="' + str(self.prefix) + '/bin/uniximage" %f', desktop)
        self.assertNotIn(str(stage), desktop)
        validator = shutil.which('desktop-file-validate')
        if validator:
            checked = run([validator, str(target / 'share/applications/uniximage.desktop')])
            self.assertEqual(checked.returncode, 0, checked.stdout + checked.stderr)

    def test_missing_gtk_auto_and_required(self):
        self.install(GUI='auto', PKG_CONFIG='/nonexistent/pkg-config')
        failed = run(['/bin/sh', str(ROOT / 'install.sh')], env=dict(
            self.env, GUI='yes', PKG_CONFIG='/nonexistent/pkg-config',
            INSTALL_PREFIX=str(self.base / 'must not exist')))
        self.assertNotEqual(failed.returncode, 0)
        self.assertFalse((self.base / 'must not exist').exists())
        self.assertFalse(list(self.base.glob('uniximage-build-*')))

    def test_compiler_flag_fallbacks(self):
        compiler = shutil.which('cc')
        wrapper = script(self.base / 'compiler', '''
case "$*" in
  *standard.c*) case " $* " in *' -std=c99 '*) ;; *) exit 1 ;; esac ;;
  *threads.c*)
    case " $* " in *' -lpthread '*) ;; *) exit 1 ;; esac
    case " $* " in *' -lrt '*) ;; *) exit 1 ;; esac ;;
esac
exec ''' + shlex.quote(compiler) + ' "$@"\n')
        result = self.install(GUI='no', CC=str(wrapper), CFLAGS='-O1')
        self.assertIn('-std=c99 -D_REENTRANT -lpthread -lrt', result.stdout)

    def test_install_elevation_uses_sudo(self):
        if os.geteuid() == 0 or os.access('/usr/bin', os.W_OK):
            self.skipTest('requires an unprivileged test user')
        fake_bin = self.base / 'tools'
        fake_bin.mkdir()
        # Verify the request, then redirect it into a writable test prefix.
        script(fake_bin / 'sudo', '''
[ "$1" = /bin/sh ]
[ "$4" = /usr ]
printf '%s\\n' called > "$TEST_LOG"
exec /bin/sh "$2" "$3" "$TEST_PREFIX" "$5"
''')
        result = run(['/bin/sh', str(ROOT / 'install.sh')], env=dict(
            self.env, GUI='no', INSTALL_PREFIX='/usr',
            PATH=str(fake_bin) + ':' + os.environ['PATH'],
            TEST_LOG=str(self.base / 'sudo.log'), TEST_PREFIX=str(self.prefix)))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'sudo.log').read_text(), 'called\n')
        self.assertTrue((self.prefix / 'bin/uniximage').exists())

    def test_gui_sudo_askpass_and_argument_boundaries(self):
        pkg = shutil.which('pkg-config') or shutil.which('pkgconf')
        if not pkg or run([pkg, '--exists', 'gtk+-3.0']).returncode:
            self.skipTest('GTK3 development files are unavailable')
        self.install(GUI='yes')
        desktop = (self.prefix / 'share/applications/uniximage.desktop').read_text()
        self.assertIn('Terminal=false', desktop)
        env_no_display = dict(self.env)
        env_no_display.pop('DISPLAY', None)
        env_no_display.pop('WAYLAND_DISPLAY', None)
        env_no_display['GDK_BACKEND'] = 'x11'
        env_no_display['DISPLAY'] = ':65534'
        result = run([str(self.prefix / 'bin/uniximage'), '--sudo-askpass', 'Password:'],
                     env=env_no_display)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')

        # Access the actual launch code without initializing GTK or writing a disk.
        harness = self.base / 'harness.c'
        harness.write_text('''
#define main uniximage_application_main
#include "uniximage.c"
#undef main
int main(int argc, char **argv) {
    privileged_write_job_t job = {0};
    if (argc != 2) return 90;
    remember_executable_path(argv[1]);
    snprintf(job.image_path, sizeof(job.image_path), "/images/quote' $file.iso");
    snprintf(job.device_path, sizeof(job.device_path), "/dev/TEST-NOT-A-DEVICE");
    snprintf(job.status_file, sizeof(job.status_file), "/tmp/test status");
    job.block_size = 4096;
    job.verify = 1;
    job.sync_writes = 1;
    job.partition_scheme = PARTscheme_GPT;
    int rc = run_privileged_cli_write(&job);
    printf("%s", job.last_output);
    return rc;
}
''')
        flags = shlex.split(run([pkg, '--cflags', '--libs', 'gtk+-3.0']).stdout)
        binary = self.base / 'harness'
        result = run(['cc', '-I', str(ROOT), str(harness), '-o', str(binary)] + flags + ['-pthread'])
        self.assertEqual(result.returncode, 0, result.stderr)
        fake_bin = self.base / 'tools'
        fake_bin.mkdir()
        writer = script(self.base / "writer's $name", '''
[ "$1" = --sudo-askpass ]
[ "$2" = 'Password for test user:' ]
[ "${CANCEL:-0}" = 0 ] || exit 1
printf '%s\\n' test-password
''')
        script(fake_bin / 'sudo', '''
[ "$1" = -A ]
[ "$2" = -- ]
printf '%s\\n' "$SUDO_ASKPASS" > "$HELPER_LOG"
password=$("$SUDO_ASKPASS" 'Password for test user:') || exit 1
[ "$password" = test-password ]
shift 2
printf '%s\\n' "$@" > "$ARG_LOG"
printf '%s\\n' writer-complete
exit "${WRITER_EXIT:-0}"
''')
        env = dict(self.env, PATH=str(fake_bin),
                   HELPER_LOG=str(self.base / 'helper.log'), ARG_LOG=str(self.base / 'args.log'))
        result = run([str(binary), str(writer)], env=env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, 'writer-complete\n')
        self.assertEqual((self.base / 'args.log').read_text().splitlines(), [
            str(writer), '--sudo-mode', '--cli', '-i', "/images/quote' $file.iso",
            '-d', '/dev/TEST-NOT-A-DEVICE', '-b', '4096', '--partition', 'gpt',
            '--status-file', '/tmp/test status', '--extra-flush', '-V', '-y'])
        helper = Path((self.base / 'helper.log').read_text().strip())
        self.assertFalse(helper.parent.exists())
        for setting, code in [('WRITER_EXIT', 7), ('CANCEL', 1)]:
            result = run([str(binary), str(writer)], env=dict(env, **{setting: str(code)}))
            self.assertEqual(result.returncode, code)
            helper = Path((self.base / 'helper.log').read_text().strip())
            self.assertFalse(helper.parent.exists())
        # Re-execution via a relative PATH still resolves to an absolute binary.
        if os.geteuid() != 0:
            script(fake_bin / 'sudo', 'printf "%s\\n" "$@" > "$ARG_LOG"\n')
            relative_env = dict(env, PATH='prefix with spaces/bin:tools:/bin:/usr/bin')
            result = run(['uniximage-cli', '-i', 'unused', '-d', 'unused'],
                         cwd=str(self.base), env=relative_env)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.base / 'args.log').read_text().splitlines()[0],
                             str(self.prefix / 'bin/uniximage-cli'))
            result = run([str(self.prefix / 'bin/uniximage-cli'), '--sudo-mode',
                          '-i', 'unused', '-d', 'unused'], env=env)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('did not grant root privileges', result.stderr)


if __name__ == '__main__':
    unittest.main()
