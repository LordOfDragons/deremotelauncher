#!/usr/bin/python3
import fnmatch, os

with open('icons.h','w') as f:
	f.write("/**\n")
	f.write(" * MIT License\n")
	f.write(" *\n")
	f.write(" * Copyright (c) 2022 DragonDreams (info@dragondreams.ch)\n")
	f.write(" *\n")
	f.write(" * Permission is hereby granted, free of charge, to any person obtaining a copy\n")
	f.write(" * of this software and associated documentation files (the \"Software\"), to deal\n")
	f.write(" * in the Software without restriction, including without limitation the rights\n")
	f.write(" * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n")
	f.write(" * copies of the Software, and to permit persons to whom the Software is\n")
	f.write(" * furnished to do so, subject to the following conditions:\n")
	f.write(" *\n")
	f.write(" * The above copyright notice and this permission notice shall be included in all\n")
	f.write(" * copies or substantial portions of the Software.\n")
	f.write(" *\n")
	f.write(" * THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n")
	f.write(" * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n")
	f.write(" * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n")
	f.write(" * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n")
	f.write(" * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n")
	f.write(" * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n")
	f.write(" * SOFTWARE.\n")
	f.write(" */\n")
	f.write("\n")
	f.write("// include only once\n")
	f.write("#ifndef _ICONS_H_\n")
	f.write("#define _ICONS_H_\n")
	f.write("\n")
	
	for root, dirs, files in os.walk('.'):
		del dirs[:]
		for s in fnmatch.filter(files, '*.bmp'):
			name = ''
			for n in s[0:-4]:
				if n.lower() in 'abcdefghijklmnopqrstuvwxyz0123456789_':
					name = name + n
				else:
					name = name + '_'
			f.write('const unsigned char icon_{}[]={{\n'.format(name))
			content = open(s,'rb').read()
			for i in range(0,len(content),16):
				f.write('  ')
				for b in content[i:i+16]:
					f.write('0x{:02x},'.format(b))
				f.write('\n')
			f.write('  };\n\n')
	
	f.write("// end of include only once\n")
	f.write("#endif\n")
