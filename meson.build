project('osc2ftdidmx', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=false',
	'b_lto=false',
	'c_std=c11'])

version = run_command('cat', 'VERSION').stdout().strip()

add_project_arguments('-DOSC2FTDIDMX_VERSION="'+version+'"', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')

osc_inc = include_directories('osc.lv2')
varchunk_inc = include_directories('varchunk')
incs = [osc_inc, varchunk_inc]

static_link = meson.is_cross_build()

thread_dep = dependency('threads')
lv2_dep = dependency('lv2', version : '>=1.14.0')
ftdi_dep = dependency('libftdi1', version : '>=1.3', static : static_link,
	required : false)
if not ftdi_dep.found()
	ftdi_dep = dependency('libftdi', version : '>=0.20', static : static_link)
else
	add_project_arguments('-DHAVE_LIBFTDI1', language : 'c')
endif

deps = [thread_dep, lv2_dep, ftdi_dep]

srcs = ['osc2ftdidmx.c']

executable('osc2ftdidmx',
	srcs,
	include_directories : incs,
	dependencies : deps,
	install : true)

configure_file(
	input : 'osc2ftdidmx.1',
	output : 'osc2ftdidmx.1',
	copy : true)

install_man(join_paths(meson.current_build_dir(), 'osc2ftdidmx.1'))

test_osc2ftdidmx = executable('test_osc2ftdidmx',
	'test.c',
	dependencies : deps,
	install : false)

test('Test', test_osc2ftdidmx)
