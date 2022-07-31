def options(opt):
	opt.add_option("--HAVE_DEBUGGER", dest="HAVE_DEBUGGER", default=0)

def configure(conf):
	conf.env.append_unique('DEFINES', 'HAVE_CIRCULAR_VERTEX_POOL')
	conf.env.append_unique('DEFINES', 'DISABLE_TEXTURE_COMBINER')

	if "release" not in conf.variant:
		conf.env.append_unique('DEFINES', 'NO_DEBUG=1')
	else:
		conf.env.append_unique('DEFINES', 'LOG_ERRORS')
		conf.env.append_unique('DEFINES', 'FILE_LOG')
		if conf.options.HAVE_DEBUGGER:
			conf.env.append_unique('DEFINES', 'HAVE_DEBUG_INTERFACE=1')

def build(bld):
	bld(
		target = "vitaGL",
		features="c cstlib",
		includes = "source",
		export_includes = "source",
		source = bld.path.ant_glob("source/**/*.c"),
	)