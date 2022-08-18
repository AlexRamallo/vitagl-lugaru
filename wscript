def options(opt):
	opt.add_option("--HAVE_DEBUGGER", dest="HAVE_DEBUGGER", default=0)

def configure(conf):
	conf.env.append_unique('DEFINES', 'HAVE_CIRCULAR_VERTEX_POOL')
	conf.env.append_unique('DEFINES', 'DISABLE_TEXTURE_COMBINER')

	if "release" in conf.cmd:
		conf.env.append_unique('DEFINES', 'SKIP_ERROR_HANDLING')
		conf.env.append_unique('DEFINES', 'MATH_SPEEDHACK')
		conf.env.append_unique('DEFINES', 'DRAW_SPEEDHACK')
	else:
		conf.env.append_unique('DEFINES', 'LOG_ERRORS')
		conf.env.append_unique('DEFINES', 'FILE_LOG')
		conf.env.append_unique('DEFINES', 'MATH_SPEEDHACK')
		conf.env.append_unique('DEFINES', 'DRAW_SPEEDHACK')
		if conf.options.HAVE_DEBUGGER:
			conf.env.append_unique('DEFINES', 'HAVE_DEBUG_INTERFACE=1')

def build(bld):
	bld(
		target = "vitaGL",
		features="c cstlib",
		includes = "source",
		export_includes = "source",
		source = bld.path.ant_glob(["source/**/*.c", "source/**/*.cpp"]),
	)