#!/usr/bin/env python3
"""Exercise the real music library/control code with deterministic SD/audio/button doubles."""
from pathlib import Path
import re, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]; SRC=ROOT/'ESP32_Tamachibi_v47_ESP32U'
source=(SRC/'MusicPlayer.cpp').read_text()
def remove_function(text,name):
    start=text.index('void Esp32MusicPlayer::'+name+'(')
    brace=text.index('{',start); depth=1; end=brace+1
    while depth:
        if text[end]=='{':depth+=1
        if text[end]=='}':depth-=1
        end+=1
    return text[:start]+'void Esp32MusicPlayer::'+name+'() const {}\n'+text[end:]
for name in ['drawBrowser','drawPlayerScreen']: source=remove_function(source,name)
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
header=re.sub(r'^#include[^\n]*\n|^#pragma once\n','',(SRC/'MusicPlayer.h').read_text(),flags=re.M).replace('private:','public:')
shim=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <new>
#include <iostream>
#include "MusicQueue.h"
class String : public std::string {
public:
 using std::string::string;
 String(const std::string &s):std::string(s){}
 String():std::string(){}
 bool startsWith(const String &s)const{return rfind(s,0)==0;}
 bool endsWith(const String &s)const{return size()>=s.size()&&compare(size()-s.size(),s.size(),s)==0;}
 void toLowerCase(){std::transform(begin(),end(),begin(),[](unsigned char c){return std::tolower(c);});}
 int lastIndexOf(char c)const{auto n=rfind(c);return n==npos?-1:int(n);}
 int indexOf(const char *s)const{auto n=find(s);return n==npos?-1:int(n);}
 String substring(unsigned a)const{return substr(a);}
 String substring(unsigned a,unsigned b)const{return substr(a,b-a);}
 int compareTo(const String &s)const{return compare(s);}
 void remove(unsigned pos){erase(pos);}
 unsigned length()const{return size();}
};
uint32_t clockMs=0;
uint32_t millis(){return clockMs;}
void delay(uint32_t ms){clockMs+=ms; assert(clockMs<20000);}
constexpr int LOW=0,HIGH=1,OUTPUT=1,INPUT=0;
void pinMode(int,int){} void digitalWrite(int,int){}
struct Press {int pin;uint32_t from,to;};std::vector<Press> presses;
int digitalRead(int pin){for(auto p:presses)if(p.pin==pin&&clockMs>=p.from&&clockMs<p.to)return LOW;return HIGH;}
long random(long n){static uint32_t seed=42;seed=seed*1664525+1013904223;return seed% n;}
long random(long low,long high){return low+random(high-low);}
struct GameDisplay{};
struct Spi{void begin(int,int,int,int){}}SPI;
struct Node{bool directory;std::vector<String> children;};
std::map<String,Node> nodes;
struct Handle{String path;size_t cursor=0;bool open=true;};
class File {
 std::shared_ptr<Handle> h;
public:
 File()=default;
 File(String path){if(nodes.count(path))h=std::make_shared<Handle>(Handle{path});}
 explicit operator bool()const{return h&&h->open;}
 bool isDirectory()const{return bool(*this)&&nodes.at(h->path).directory;}
 String name()const{return h?h->path.substring(h->path.lastIndexOf('/')+1):String();}
 File openNextFile(){if(!isDirectory())return File();auto &c=nodes.at(h->path).children;return h->cursor<c.size()?File(c[h->cursor++]):File();}
 void close(){if(h)h->open=false;}
};
struct Sd{
 bool mounted=true;
 bool begin(int,Spi&,uint32_t,const char*,int){return mounted;}
 void end(){}
 File open(const String &p){return File(p);}
}SD;
void add(const String &path,bool directory=false){
 nodes[path]={directory,{}};
 int slash=path.lastIndexOf('/');String parent=path.substring(0,slash);
 if(nodes.count(parent))nodes[parent].children.push_back(path);
}
struct CareOled{
 std::map<int,String> rows;int row=0;
 void clearDisplay(){rows.clear();}void setTextColor(int){}void setTextSize(int){}void setTextWrap(bool){}
 void setCursor(int,int y){row=y;}void print(const String&s){rows[row]=s;}void display(){}
};
#define SSD1306_WHITE 1
struct NeedsDisplay{
 bool ready_=true;String musicTitle_;uint32_t musicTitleSince_=0;CareOled oled_;
 void showMusic(const String&,const String&);
};
int liveSources=0,liveBuffers=0,liveDecoders=0,liveOutputs=0,starts=0,loops=0;
bool decoderFails=false;int finishAfter=-1;
struct AudioFileSourceSD{
 bool opened=false;
 AudioFileSourceSD(){++liveSources;}~AudioFileSourceSD(){--liveSources;}
 bool open(const char*p){return opened=nodes.count(String(p));}
 void close(){opened=false;}
};
struct AudioFileSourceBuffer{
 AudioFileSourceSD *src;
 AudioFileSourceBuffer(AudioFileSourceSD*s,void*,size_t):src(s){++liveBuffers;}
 ~AudioFileSourceBuffer(){--liveBuffers;}void close(){src->close();}
};
struct AudioOutput{
 virtual ~AudioOutput()=default;
 virtual bool SetGain(float){return true;}virtual bool stop(){return true;}
};
struct AudioOutputI2S:AudioOutput{
 AudioOutputI2S(){++liveOutputs;}~AudioOutputI2S(){--liveOutputs;}
 void SetBuffers(int,int){}void SetPinout(int,int,int){}bool SetGain(float)override{return true;}bool stop()override{return true;}
};
struct BluetoothAudioOutput:AudioOutput{
 bool begin(){return true;}bool SetRate(int){return true;}bool SetChannels(int){return true;}
 bool SetGain(float)override{return true;}bool ConsumeSample(int16_t[2]){return true;}
 bool stop()override{return true;}bool isConnected()const{return true;}
};
struct AudioGeneratorMP3{
 bool running=false;
 AudioGeneratorMP3(){++liveDecoders;}~AudioGeneratorMP3(){--liveDecoders;}
 bool begin(AudioFileSourceBuffer*,AudioOutput*){++starts;return running=!decoderFails;}
 bool isRunning()const{return running;}void stop(){running=false;}
 bool loop(){++loops;if(finishAfter>=0&&loops>=finishAfter)running=false;return running;}
};
'''
source+='\nvoid NeedsDisplay::showMusic'+(SRC/'NeedsDisplay.cpp').read_text().split('void NeedsDisplay::showMusic',1)[1]
tests=r'''
void clean(){assert(!liveSources&&!liveBuffers&&!liveDecoders&&!liveOutputs);}
int main(){
 // Actual care-display method strips MP3 suffixes, scrolls, and resets for a new title.
 NeedsDisplay care;clockMs=100;care.showMusic("A very long song name for scrolling.MP3","PLAYING");
 assert(care.oled_.rows[11]=="A very long song name");
 clockMs=450;care.showMusic("A very long song name for scrolling.MP3","PLAYING");
 assert(care.oled_.rows[11]==" very long song name ");
 clockMs=800;care.showMusic("New song.mp3","PAUSED");assert(care.oled_.rows[11]=="New song     New song");
 assert(care.oled_.rows[23]=="PAUSED");
 // 128-track boundary, shuffle coverage and reversible Previous history.
 for(uint32_t seed=1;seed<50;++seed){
   MusicQueue q;for(int i=0;i<130;++i)q.add(i);assert(q.count()==128);
   q.shuffle(seed,63);assert(q.current()==63);
   bool seen[128]={};for(int i=0;i<128;++i){assert(!seen[q.current()]);seen[q.current()]=true;if(i<127)assert(q.step(1,false));}
   assert(!q.step(1,false));assert(q.step(-1,false));auto previous=q.current();assert(q.step(1,false));assert(q.step(-1,false));assert(q.current()==previous);
   assert(q.step(1,true));assert(q.step(1,true));assert(q.current()==63);
 }
 MusicQueue empty;assert(!empty.step(1,true));empty.shuffle(1,0);
 GameDisplay display;Esp32MusicPlayer p(display,1,2,3,4,5);
 add("/music",true);add("/music/01_root.mp3");add("/music/Album",true);
 add("/music/Album/02_song.MP3");add("/music/Album/notes.txt");
 add("/music/.hidden",true);add("/music/.hidden/no.mp3");add("/music/._ghost.mp3");
 add("/music/Album/Nested",true);add("/music/Album/Nested/03_song.mp3");
 assert(p.scanMusicDirectory());assert(p.trackCount_==3);assert(p.folderCount()==4);
 assert(p.folderAt(0)=="");assert(p.folderAt(1)=="/music");
 p.folder_="/music/Album";p.rebuildQueue();assert(p.queue_.count()==1);
 assert(p.trackName(p.selectedTrack_)=="02_song.MP3");assert(!p.advance(1,false));
 p.startPlayback();assert(p.isPlaying());assert(starts==1);
 p.stopPlayback("STOPPED");clean();
 decoderFails=true;p.startPlayback();assert(p.status_=="MP3 START FAILED");clean();decoderFails=false;
 // Removed cards clear the playable queue; hold Right exits even on errors.
 SD.mounted=false;presses={{3,100,1150}};clockMs=0;p.run();assert(p.trackCount_==0);assert(p.queue_.count()==0);clean();SD.mounted=true;
 // Short Select plays; long Select opens options without accidentally pausing.
 // Choosing Exit stops and frees all audio objects.
 presses={{2,100,180},{2,250,1250},{1,1350,1430},{2,1500,1580}};clockMs=0;p.run();assert(starts==3);clean();
 // Pause/resume preserves the same decoder, rather than reopening the file.
 int before=starts;
 presses={{2,100,180},{2,250,330},{2,400,480},{3,600,1600}};clockMs=0;p.run();assert(starts==before+1);clean();
 // Automatic order reaches the final song and stops, without wrapping when repeat is off.
 p.repeat_=0;p.shuffle_=false;loops=0;finishAfter=1;
 presses={{2,100,180},{3,1000,2000}};clockMs=0;before=starts;p.run();assert(starts==before+3);assert(p.selectedTrack_==2);clean();finishAfter=-1;
 // Folder browser: select the root-only collection, then play it.
 presses={{2,100,1100},{3,1200,1280},{3,1350,1430},{2,1500,1580},
          {3,1650,1730},{2,1800,1880},{2,1950,2030},{3,2200,3200}};
 clockMs=0;p.run();assert(p.folder_=="/music");assert(p.queue_.count()==1);clean();
 // Repeat One reopens the same track; Repeat All wraps the collection.
 for(int mode=1;mode<=2;++mode){
   p.repeat_=mode;loops=0;finishAfter=1;before=starts;
   presses={{2,100,180},{3,500,1500}};clockMs=0;p.run();
   assert(starts>before+3);clean();
 }
 p.repeat_=0;finishAfter=-1;
 // Library overflow is reported and cannot overrun the fixed track storage.
 nodes.clear();add("/music",true);
 for(int i=0;i<140;++i)add(String("/music/")+std::to_string(i)+".mp3");
 assert(p.scanMusicDirectory());assert(p.trackCount_==128);assert(p.libraryLimited_);
 // Deep paths are bounded without preventing ordinary songs from being indexed.
 nodes.clear();add("/music",true);add("/music/base.mp3");String deep="/music";
 for(int i=0;i<9;++i){deep+="/level";add(deep,true);}add(deep+"/too_deep.mp3");
 assert(p.scanMusicDirectory());assert(p.trackCount_==1);assert(p.libraryLimited_);
 // Empty and missing music roots remain navigable, with clear messages.
 nodes.clear();assert(!p.scanMusicDirectory());assert(p.status_=="CREATE /music FOLDER");
 add("/music",true);assert(!p.scanMusicDirectory());assert(p.status_=="NO MP3 IN /music");
 std::cout<<"Music checks passed: folders, 128-track limit, shuffle coverage/history, pause/resume, EOF, exit, card errors and audio cleanup\n";
}
'''
with tempfile.TemporaryDirectory() as td:
    cpp=Path(td)/'music.cpp';cpp.write_text(shim+'\n'+header+'\n'+source+'\n'+tests)
    exe=Path(td)/'checks'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(SRC),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

