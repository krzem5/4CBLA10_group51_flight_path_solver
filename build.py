############################################################################
# Copyright (c) Krzesimir Hyżyk - All Rights Reserved                      #
# Unauthorized copying of this file, via any medium is strictly prohibited #
# Proprietary and confidential                                             #
# Created on 18/11/2024 by Krzesimir Hyżyk                                 #
############################################################################

from PIL import Image,ImageDraw
import array
import os
import subprocess
import sys



GRAPH_SPACE_SCALE=1941.8223137449165



def _get_source_files(*directories):
	for directory in directories:
		for root,_,files in os.walk(directory):
			for file in files:
				if (file.endswith(".asm") or file.endswith(".c")):
					yield os.path.join(root,file)



def _generate_flight_graph(src_file_path,dst_file_path,scale,best_only=False):
	bbox=[0,0,0,0]
	last_layer=None
	files=sorted(list(os.listdir(src_file_path)))
	for k in files[:]:
		if ("." in k):
			files.remove(k)
			continue
		with open(src_file_path+"/"+k,"rb") as rf:
			data=array.array("d",rf.read())
		for i in range(0,len(data),4):
			if (data[i+1]<0):
				break
			if (data[i]<bbox[0]):
				bbox[0]=data[i]
			elif (data[i]>bbox[2]):
				bbox[2]=data[i]
				last_layer=k
			if (data[i+1]<bbox[1]):
				bbox[1]=data[i+1]
			elif (data[i+1]>bbox[3]):
				bbox[3]=data[i+1]
	if (best_only):
		files=[last_layer]
		last_layer=None
	else:
		files.remove(last_layer)
		files.append(last_layer)
	print(bbox,last_layer)
	border=(bbox[2]-bbox[0])/10
	bbox[0]-=border
	bbox[2]+=border
	border=(bbox[3]-bbox[1])/10
	bbox[1]-=border
	bbox[3]+=border
	image=Image.new("RGB",(round((bbox[2]-bbox[0])*scale),round((bbox[3]-bbox[1])*scale)),"#ffffff")
	draw=ImageDraw.Draw(image)
	draw.line((-bbox[0]*scale,0,-bbox[0]*scale,image.height),fill="#909090",width=10)
	draw.line((0,bbox[3]*scale,image.width,bbox[3]*scale),fill="#909090",width=10)
	for k in files:
		print(k)
		with open(src_file_path+"/"+k,"rb") as rf:
			data=array.array("d",rf.read())
		color=("#ffa348" if k==last_layer else "#62a0ea")
		last_point=(0,0)
		for i in range(0,len(data),4):
			if (data[i+1]<0):
				break
			x=round((data[i]-bbox[0])*scale)
			y=round((bbox[3]-data[i+1])*scale)
			if (x==last_point[0] and y==last_point[1]):
				continue
			draw.rectangle((x-3,y-3,x+3,y+3),fill=color)
			last_point=(x,y)
	image.save(dst_file_path)



if (not os.path.exists("build")):
	os.mkdir("build")
if (os.path.exists("build/points")):
	for k in os.listdir("build/points"):
		os.unlink(f"build/points/{k}")
else:
	os.mkdir("build/points")
source_file_directory=("src/eta_predictor" if "--predict-eta" in sys.argv else "src/solver")
if ("--release" in sys.argv):
	object_files=[]
	error=False
	for file in _get_source_files(source_file_directory):
		object_file=f"build/{file.replace('/','$')}.o"
		object_files.append(object_file)
		if (file.endswith(".c")):
			if (subprocess.run(["gcc","-Wall","-lm","-Werror","-march=native","-mno-red-zone","-Wno-strict-aliasing","-mno-avx256-split-unaligned-load","-ffast-math","-momit-leaf-frame-pointer","-Ofast","-g0","-c","-D_GNU_SOURCE","-DNULL=((void*)0)",file,"-o",object_file,f"-I{source_file_directory}/include"]).returncode!=0):
				error=True
		elif (file.endswith(".asm")):
			if (subprocess.run(["nasm","-f","elf64","-O3","-Wall","-Werror","-W-reloc-rel-dword","-W-reloc-rel-qword","-W-reloc-abs-dword","-W-reloc-abs-qword","-W-db-empty",file,"-o",object_file]).returncode!=0):
				error=True
	if (error or subprocess.run(["gcc","-O3","-g0","-o","build/solver","-X","-znoexecstack"]+object_files+["-lm","-pthread"]).returncode!=0):
		sys.exit(1)
else:
	object_files=[]
	error=False
	for file in _get_source_files(source_file_directory):
		object_file=f"build/{file.replace('/','$')}.o"
		object_files.append(object_file)
		if (file.endswith(".c")):
			if (subprocess.run(["gcc","-Wall","-lm","-Werror","-march=native","-mno-red-zone","-Wno-strict-aliasing","-O0","-ggdb","-c","-D_GNU_SOURCE","-DNULL=((void*)0)",file,"-o",object_file,f"-I{source_file_directory}/include"]).returncode!=0):
				error=True
		elif (file.endswith(".asm")):
			if (subprocess.run(["nasm","-f","elf64","-O0","-Wall","-Werror","-W-reloc-rel-dword","-W-reloc-rel-qword","-W-reloc-abs-dword","-W-reloc-abs-qword","-W-db-empty",file,"-o",object_file]).returncode!=0):
				error=True
	if (error or subprocess.run(["gcc","-O0","-ggdb","-o","build/solver","-X","-znoexecstack"]+object_files+["-lm","-pthread"]).returncode!=0):
		sys.exit(1)
if ("--run" in sys.argv):
	subprocess.run(["build/solver"])
	if ("--predict-eta" not in sys.argv):
		_generate_flight_graph("build/points","build/trajectory.png",256*GRAPH_SPACE_SCALE)
