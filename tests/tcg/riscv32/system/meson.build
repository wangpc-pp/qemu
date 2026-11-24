# SPDX-License-Identifier: GPL-2.0-or-later

tests = []

link_script = files('../semihost.ld')[0]
cflags = ['-march=rv32im_zicsr',
          '-mabi=ilp32',
          '-nostdlib',
          '-ffreestanding',
          '-Wa,--noexecstack',
          '-Wl,-T', link_script]
qemu_args = ['-M', 'virt',
             '-display', 'none',
             '-serial', 'stdio',
             '-semihosting',
             '-bios']

tests += {
  'test-mcycle.S': {
    'cflags': cflags,
    'qemu_args': ['-icount', 'shift=1', qemu_args],
  },
}

if 'qemu-system-riscv32' in emulators
  foreach compressed : [false, true]
    zibs_cpu = ('rv32,x-zibs=true,c=' +
                (compressed ? 'true' : 'false,zca=false'))
    zibs_setup = {
      'cflags': cflags + (compressed ? ['-DZIBS_COMPRESSED'] : []),
      'qemu_args': ['-cpu', zibs_cpu, qemu_args],
    }
    if compressed
      zibs_setup += {'exe_name': 'test-zibs-c'}
    endif
    tests += {
      '../riscv64/test-zibs.S': zibs_setup,
    }
  endforeach
  tests += {
    '../riscv64/test-zibs.S': {
      'exe_name': 'test-zibs-disabled',
      'cflags': cflags + ['-DZIBS_DISABLED'],
      'qemu_args': ['-cpu', 'rv32,x-zibs=false', qemu_args],
    },
  }
  tests += {
    '../riscv64/test-zibs.S': {
      'exe_name': 'test-zibs-disabled',
      'test_name': 'test-zibs-default',
      'qemu_args': ['-cpu', 'rv32', qemu_args],
    },
  }

  tcg_tests += {
    'riscv32-softmmu': {
      'cc': 'riscv64-linux-gnu-gcc',
      'cc_dockerfile': 'debian-all-test-cross',
      'folder': 'riscv32',
      'gdb_arch': 'riscv32',
      'qemu': emulators['qemu-system-riscv32'],
      'tests': tests,
    }
  }
endif
