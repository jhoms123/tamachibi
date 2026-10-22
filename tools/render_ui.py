#!/usr/bin/env python3
"""Render real firmware drawing functions with Adafruit GFX on a host computer.
Run PlatformIO dependency installation first, then: python tools/render_ui.py
Only the SH1107 transport and Arduino Print/time glue are mocked.
"""
from pathlib import Path
import argparse,re,subprocess,tempfile
from PIL import Image,ImageDraw,ImageFont
import math
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT/'ESP32_Tamachibi_v47_ESP32U'
def extract(file,name):
 s=(SRC/file).read_text();mask=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),s,flags=re.S)
 m=re.search(r'(?m)^void\s+'+re.escape(name)+r'\s*\([^;{}]*\)\s*(?:const\s*)?\{',mask)
 if not m:raise ValueError(name)
 pos=m.end();depth=1
 while depth:
  if mask[pos]=='{':depth+=1
  if mask[pos]=='}':depth-=1
  pos+=1
 return s[m.start():pos]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--gfx',type=Path,default=SRC/'.pio/libdeps/esp32u/Adafruit GFX Library');args=ap.parse_args();gfx=args.gfx
 if not (gfx/'Adafruit_GFX.cpp').exists():raise SystemExit('Adafruit GFX missing; run PlatformIO or pass --gfx PATH')
 program=(ROOT/'tools/preview_fixture.cpp').read_text()
 for f,names in {
  'GameMain.cpp':['drawSquarePet','drawMenu','drawHouseWorldView','drawCityWorldView'],
  'ParentDialogue.cpp':['ParentDialogueSystem::drawDialoguePage','ParentDialogueSystem::drawChoicePage','ParentDialogueSystem::drawResultPage'],
  'Aquarium.cpp':['AquariumSystem::drawTankScene'],
  'Botany.cpp':['BotanySystem::drawGardenOverview','BotanySystem::drawPlantRoomOverview'],
  'MusicPlayer.cpp':['Esp32MusicPlayer::drawPlayerScreen']}.items():
  for name in names:program+='\n'+extract(f,name)+'\n'
 for func,vars in [('categoryMenu','byte pick=2,count=6,category=0;'),('miniGameMenu','byte g=0,gameCount=14;'),('gameHubMenu','byte selected=1;')]:
  body=extract('GameMain.cpp',func);mark=body.index('NativeUI::Scope frame(display)');start=body.rfind('\n    {',0,mark)+1
  mask=re.sub(r'"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),body);pos=start;depth=0
  while pos<len(body):
   if mask[pos]=='{':depth+=1
   if mask[pos]=='}':
    depth-=1
    if depth==0:pos+=1;break
   pos+=1
  program+='\nvoid fixture_'+func+'(){'+vars+body[start:pos]+'}\n'
 program+=r'''
int main(){
 auto save=[](const char *s){display.display();panel.save(s);};
 {NativeUI::Scope f(display);display.clearDisplay();drawSquarePet();save("home.pbm");}
 {NativeUI::Scope f(display);display.clearDisplay();mainCategory=2;drawMenu();save("menu.pbm");}
 {NativeUI::Scope f(display);display.clearDisplay();drawHouseWorldView();save("house.pbm");}
 {NativeUI::Scope f(display);display.clearDisplay();drawCityWorldView();save("town.pbm");}
 fixture_categoryMenu();save("actions.pbm");fixture_miniGameMenu();save("arcade.pbm");fixture_gameHubMenu();save("game-types.pbm");
 ParentDialogueSystem p;ParentDialogueContext c;ParentDialogueOutcome o;
 p.drawDialoguePage(c,"I found a tiny flower today. Can we grow something together?",1,3);save("talk.pbm");
 p.drawChoicePage(c,0,1);save("choices.pbm");
 p.drawResultPage(c,"That sounds lovely. I feel braver when you help me try.",o);save("reply.pbm");
 AquariumSystem a;a.drawTankScene(0,10,true);save("tank.pbm");
 BotanySystem b;b.drawGardenOverview(0);save("garden.pbm");b.drawPlantRoomOverview(0);save("plants.pbm");
 Esp32MusicPlayer music;music.drawPlayerScreen();save("music.pbm");
 display.useNativeLayout(false);display.clearDisplay();display.setCursor(20,0);display.print("LONG STATUS TITLE");display.setCursor(105,0);display.print("2/7");
 display.setCursor(1,14);display.print("School confidence 100");display.setCursor(1,26);display.print("Social confidence 86");display.setCursor(1,38);display.print("Attachment security 95");display.setCursor(1,50);display.print("Difficult talks 32");display.setPageFooter("D0 PREV","D1 OPEN","D2 NEXT");save("status.pbm");
 display.clearDisplay();display.setPageTitle("SAVE PROFILES","1/9");display.setCursor(12,24);display.print("Create restore point");display.setCursor(12,38);display.print("MOCHI G12 D366");display.setPageFooter("PREV","SAVE","NEXT");save("save.pbm");
 // Prove captured text is excluded from the separate art framebuffer.
 display.useGameCanvas(true);display.setGameButtons("LEFT","HOP","RIGHT");display.clearDisplay();display.setPageTitle("WALK","24");
 for(int y=14;y<52;y+=10){display.drawLine(0,y,127,y,1);display.drawRoundRect((y*3)%95,y+1,14,6,2,1);}
 NativeUI::pet(display,60,37,0,4,0);display.setCursor(2,10);display.print("LEVEL 3");display.setPageFooter("LEFT","HOP","RIGHT");save("game.pbm");
 return 0;
}
'''
 out=ROOT/'docs/native-ui';out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory() as td:
  td=Path(td);(td/'preview.cpp').write_text(program)
  cmd=['g++','-std=c++17','-DARDUINO=100','-I'+str(ROOT/'tools/host'),'-I'+str(gfx),'-I'+str(SRC),str(gfx/'Adafruit_GFX.cpp'),str(SRC/'GameDisplay.cpp'),str(SRC/'NativeUI.cpp'),str(td/'preview.cpp'),'-o',str(td/'preview')]
  subprocess.run(cmd,check=True);subprocess.run([str(td/'preview')],cwd=td,check=True)
  checks=cmd[:-3]+[str(ROOT/'tools/renderer_checks.cpp'),'-o',str(td/'checks')]
  subprocess.run(checks,check=True);subprocess.run([str(td/'checks')],check=True)
  names=['home','menu','actions','arcade','game-types','house','town','talk','choices','reply','tank','garden','plants','music','status','save','game']
  scale=3;cellw,cellh=416,438;sheet=Image.new('RGB',(cellw*4,cellh*math.ceil(len(names)/4)),'#191d22');draw=ImageDraw.Draw(sheet)
  font=ImageFont.truetype('DejaVuSans.ttf',17)
  for i,n in enumerate(names):
   # PBM stores black as 1; fixture writes white OLED pixels as 0.
   image=Image.open(td/(n+'.pbm')).convert('RGB');image.save(out/(n+'.png'))
   x=(i%4)*cellw+16;y=(i//4)*cellh+32
   sheet.paste(image.resize((384,384),Image.Resampling.NEAREST),(x,y));draw.text((x,y-25),n.upper(),font=font,fill='white')
  sheet.save(out/'overview.png')
  highlights=Image.new('RGB',(cellw*4,cellh),'#191d22');hd=ImageDraw.Draw(highlights)
  for i,n in enumerate(['home','menu','town','choices']):
   im=Image.open(out/(n+'.png'));x=i*cellw+16;highlights.paste(im.resize((384,384),Image.Resampling.NEAREST),(x,32));hd.text((x,7),n.upper(),font=font,fill='white')
  highlights.save(out/'highlights.png')
 print(f'Rendered {len(names)} firmware fixtures: {out}/overview.png')
if __name__=='__main__':main()
