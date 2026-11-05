from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import arabic_reshaper
from bidi.algorithm import get_display
import subprocess,io,json,hashlib,argparse

ROOT=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('video',type=Path)
parser.add_argument('--font-dir',type=Path,default=Path.home()/'Library/Fonts')
parser.add_argument('--preview-output',type=Path,default=ROOT/'preview')
args=parser.parse_args()
SOURCE=args.video.resolve()
OUTPUT=args.preview_output.resolve()
OUTPUT.mkdir(parents=True,exist_ok=True)
ASSETS=ROOT/'assets';ASSETS.mkdir(exist_ok=True)
FRAMES=ASSETS/'frames';FRAMES.mkdir(exist_ok=True)
FONT=args.font_dir/'Vazirmatn-Regular.ttf'
BOLD=args.font_dir/'Vazirmatn-Bold.ttf'
SCALE=3
panel=Image.new('RGB',(224*SCALE,170*SCALE),(12,18,27))
d=ImageDraw.Draw(panel)
def text(value,y,size,color,bold=False,rtl=True):
    font=ImageFont.truetype(str(BOLD if bold else FONT),size*SCALE)
    shaped=get_display(arabic_reshaper.reshape(value)) if rtl else value
    box=d.textbbox((0,0),shaped,font=font)
    x=(224*SCALE-(box[2]-box[0]))/2
    d.text((x,y*SCALE-box[1]),shaped,font=font,fill=color)
text('رفیـــــق،',24,30,(255,196,111),True)
text('این از سرت افتاده.',78,20,(246,243,235))
text('بگیر بذار سر جاش',108,20,(246,243,235))
text(':)))))',142,17,(255,196,111),rtl=False)
panel=panel.resize((224,170),Image.Resampling.LANCZOS)
panel.save(ASSETS/'panel.png')
buf=io.BytesIO();panel.save(buf,format='JPEG',quality=94,subsampling=0)
paneldata=buf.getvalue()
for old in FRAMES.glob('*.jpg'):old.unlink()
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(SOURCE),'-an','-vf','fps=10,scale=96:170:flags=lanczos','-q:v','7',str(FRAMES/'%04d.jpg')],check=True)
frames=sorted(FRAMES.glob('*.jpg'));blob=bytearray();index=[]
for frame in frames:
    payload=frame.read_bytes();index.append((len(blob),len(payload)));blob.extend(payload)
header='''// Generated from refigh.mp4. Silent MJPEG; Persian text rasterized with Vazirmatn.
#pragma once
#include <Arduino.h>
struct RefighFrame { uint32_t offset; uint32_t length; };
constexpr uint16_t REFIGH_FRAME_COUNT = %d;
constexpr uint16_t REFIGH_FRAME_MS = 100;
'''%len(index)
def array(name,data):
    return 'const uint8_t '+name+'[] PROGMEM = {\n'+''.join('  '+','.join(f'0x{x:02x}' for x in data[i:i+24])+',\n' for i in range(0,len(data),24))+'};\n'
header+=array('refighPanel',paneldata)+array('refighVideo',blob)
header+='const RefighFrame refighFrames[] = {\n'+''.join('  {%d,%d},\n'%x for x in index)+'};\n'
(ROOT/'ats-mini/RefighAssets.h').write_text(header)
previewframes=ASSETS/'preview';previewframes.mkdir(exist_ok=True)
for old in previewframes.glob('*.png'):old.unlink()
for i,frame in enumerate(frames):
    image=Image.new('RGB',(320,170));image.paste(Image.open(frame),(0,0));image.paste(panel,(96,0))
    image.save(previewframes/f'{i:04d}.png')
    if i==20:image.save(OUTPUT/'refigh-preview.png')
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-framerate','10','-i',str(previewframes/'%04d.png'),'-an','-c:v','libx264','-pix_fmt','yuv420p','-crf','20','-movflags','+faststart',str(OUTPUT/'refigh-preview.mp4')],check=True)
meta=dict(source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),source_bytes=SOURCE.stat().st_size,frames=len(index),fps=10,video_bytes=len(blob),text_panel_bytes=len(paneldata),total_embedded_bytes=len(blob)+len(paneldata),display=[320,170],text='رفیـــــق، این از سرت افتاده. بگیر بذار سر جاش :)))))',font='Vazirmatn',audio=False)
(OUTPUT/'media-info.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2))
print(json.dumps(meta,ensure_ascii=False,indent=2))
