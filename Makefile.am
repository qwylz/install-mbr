# This is a hack, but automake changes in incompatible ways.
AUTOMAKE=automake-1.9
ACLOCAL=aclocal-1.9
AUTOCONF=autoconf2.50
AUTOHEADER=autoheader2.50

if HAVE_VM86
SUBDIRS = harness
else
SUBDIRS =
endif

sbin_PROGRAMS = install-mbr
install_mbr_SOURCES = install-mbr.c mbr.S86 mbr.h
install_mbr_LDADD = mbr.o y2k.o
noinst_DATA = testdisk.b
noinst_SCRIPTS = wraptest

AS86 = as86
LD86 = ld86

AM_CFLAGS = -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror

man_MANS = install-mbr.8

TESTS = tests/version tests/inst-1 tests/inst-2 tests/inst-3 \
    tests/inst-4 tests/inst-5 tests/inst-6 tests/inst-7 tests/inst-8 \
    tests/mbr-1 tests/mbr-2 tests/mbr-3 tests/mbr-4 \
    tests/mbr-5 tests/mbr-6 \
    tests/testdisk-1 tests/testdisk-2 tests/testdisk-3 \
    tests/testdisk-4 tests/testdisk-5 tests/testdisk-6
TESTS_ENVIRONMENT = sh wraptest

check_DATA = \
  table.b \
  mbr.b \
  mbr-1.1.3.b \
  mbr-1.1.2.b \
  mbr-1.1.1.b \
  mbr-1.1.0.b \
  mbr-1.0.1.b \
  mbr-1.0.0.b

EXTRA_DIST = table.S86 historic,v ${TESTS} ${man_MANS}

CLEANFILES = *.b *.s86 *.o86 *.lst *.sym test.* test-* \
  mbr-?.?.?.S86

SUFFIXES = .b .S86 .s86

.PRECIOUS: mbr.b y2k.b testdisk.b

%.s86: %.S86
	$(CC) -E -xc $< -o $@.tmp && mv $@.tmp $@

mbr-1.0.0.S86: historic,v
	co -pMBR_1_0_0 $< >$@
mbr-1.0.1.S86: historic,v
	co -pMBR_1_0_1 $< >$@
mbr-1.1.0.S86: historic,v
	co -pMBR_1_1_0 $< >$@
mbr-1.1.1.S86: historic,v
	co -pMBR_1_1_1 $< >$@
mbr-1.1.2.S86: historic,v
	co -pMBR_1_1_2 $< >$@
mbr-1.1.3.S86: historic,v
	co -pMBR_1_1_3 $< >$@

y2k.b: y2k.s86
y2k.s86: mbr.S86
	$(CC) -DY2KBUG -E -xc $< -o $@.tmp && mv $@.tmp $@

testdisk.b: testdisk.s86
testdisk.s86: mbr.S86
	$(CC) -DTESTDISK -E -xc $< -o $@.tmp && mv $@.tmp $@

%.b: %.s86
	$(AS86) -0 -b $@.tmp -s $*.sym -l $*.lst $< && mv $@.tmp $@

%.o: %.b
	$(LD) -r -b binary -o $@ $<

%.o86: %.s86
	$(AS86) -0 -w -u -o $@.tmp -l $*.lst $< && mv $@.tmp $@
