#!/usr/bin/env python3
"""Run real firmware dispatch/completion code with deterministic activity/UI doubles.
Also checks cooldown, terminal arcade results, permissions and save-layout-safe policy.
No Arduino/board tools are needed: python3 tools/check_gameplay.py
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'ESP32_Tamachibi_v47_ESP32U'
source = (SRC / 'GameMain.cpp').read_text()

def extract(text, name):
    mask = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ' ' * len(m[0]), text, flags=re.S)
    found = re.search(r'(?m)^(?:bool|void|uint8_t|byte|const __FlashStringHelper\*)\s+' + re.escape(name) + r'\([^;{}]*\)\s*\{', mask)
    if not found:
        raise AssertionError(name)
    depth, end = 1, found.end()
    while depth:
        if mask[end] == '{': depth += 1
        if mask[end] == '}': depth -= 1
        end += 1
    return text[found.start():end]

# The sitter must return before schedules, ages, fish/plants, and narrative ticks.
tick = extract(source, 'gameTick')
assert tick.index('if (sitterTicksRemaining > 0)') < tick.index('synchronizeDailySchedule')
assert tick.index('if (sitterTicksRemaining > 0)') < tick.index('aquarium.tick') < tick.index('ageTicks++')
family = extract(source, 'familyPage')
assert 'partnerBond + 5' not in family and 'partnerId = pick' not in family
assert 'completeGameplayAction' not in extract(source, 'executeAction')
# No completion from constructors/reset paths.
for file, name in [('Aquarium.cpp', 'AquariumSystem::reset'), ('Botany.cpp', 'BotanySystem::reset')]:
    assert 'completeGameplayAction' not in extract((SRC / file).read_text(), name)

stage = re.search(r'enum GrowthStage : byte \{.*?\};', source, re.S)[0]
growing = re.search(r'enum GrowingAction : uint8_t \{.*?\};', (SRC/'GrowingTogether.h').read_text(), re.S)[0]
program = r'''
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include "GameplayActions.h"
#include "GameplayProgress.h"
using byte = uint8_t;
using __FlashStringHelper = char;
#define F(x) x
using std::min;
using std::max;
'''+stage+'\n'+growing+r'''
bool dead=false, sleeping=false, petAway=false, away=false, activitySucceeds=false;
unsigned long ageTicks=100, sitterTicksRemaining=0, lastCareTick=0, lastTalkTick=0;
unsigned int generation=1, coins=0;
byte stage=STAGE_ADULT, menu=0, ownerBond=90, ownerTrust=90, energy=8, happy=5;
byte dailyRoutineMask=0, exerciseScore=0, weight=5;
constexpr byte ROUTINE_GREET=1, ROUTINE_CHECK=2, ROUTINE_CARE=4, ROUTINE_TALK=8, ROUTINE_PLAY=16, ROUTINE_BED=32;
GameplayProgress gameplayProgress;
int living=0, deep=0, narratives[14]={}, callbacks=0, resultCallbacks=0;
int lastResult=-1;
byte naturalActivity(){return M_STUDY;}
unsigned int petAgeDays(){return ageTicks/288;}
byte visualHour(){return 16;} byte visualWeather(){return 0;}
bool isScheduledAwayTime(){return away;}
bool isScheduledSchoolTime(){return away && stage<4;}
long random(long,long){return 1;}
void addCoins(byte n){coins+=n;}
void changeBond(int n){ownerBond=std::min(100,int(ownerBond)+n);}
void changeTrust(int n){ownerTrust=std::min(100,int(ownerTrust)+n);}
void addStat(byte &v, byte n){v=std::min(10,int(v)+n);}
void markSaveDirty(){} void drawScreen(bool){} void badSound(){}
void popup(const char*,const char*,int=0){} void waitButtonRelease(){}
void updateWorldViewFromEncoder(bool){}
void recordGrowingAction(uint8_t a){++narratives[a];}
void logCareEvent(byte){} void recordGoalEvent(byte,byte){}
struct World {
  void recordAction(byte,unsigned long,byte,byte){++living;}
  void recordGameResult(byte,bool won){++resultCallbacks;lastResult=won;}
  void runHub(){++callbacks;}
} lifeWorld;
struct Deep {
 void recordAction(byte,unsigned long,byte,byte){++deep;}
 void recordGameResult(byte,bool won,byte){++resultCallbacks;lastResult=won;}
} deepLife;
struct Botany {
 void runGarden(){++callbacks;if(activitySucceeds)completeGameplayAction(M_GARDEN);}
 void runPlantRoom(){++callbacks;if(activitySucceeds)completeGameplayAction(M_PLANTROOM);}
} botany;
struct City {
 byte room=3,place=4;
 byte houseRoom(){return room;} byte cityPlace(){return place;}
 void visitResidents(){++callbacks;}
} cityWorld;
constexpr byte HOUSE_ROOM_COUNT=5,CITY_PLACE_COUNT=6;
void drawMusicPlayerScene(bool){} void drawMusicPlayerInfo(int,int){}
unsigned long millis(){return 0;}
int makeNeedsDisplayState(){return 0;}
struct Needs {void update(int,unsigned long,bool){}} needsDisplay;
struct Music {template<class A,class B>void setViews(A,B){} void run(){++callbacks;}} musicPlayer;
void exploreTrip(){++callbacks;if(energy<2||!activitySucceeds)return;completeGameplayAction(M_EXPLORE);}
void jobActivity(){++callbacks;if(energy<2||!activitySucceeds)return;completeGameplayAction(M_JOB);}
'''
# Navigation pages and cancelled activity doubles never emit completion.
for name in ['startReplacementEgg','doFood','doLight','gameHubMenu','doMedicine','doClean','doLove','doCall',
             'walkGame','trainGame','studyGame','musicGame','shopClothes','toyActivity','cookActivity',
             'roomActivity','aquariumActivity','talkActivity','statusPage','settingsMenu','albumPage',
             'sitterModePage','tamaBookPage','craftPage','habitatPage','seasonalEventPage','familyPage',
             'inspectPage','careHistoryPage','diaryPage','diagnosticsPage','paradiseLabPage']:
    program += 'void '+name+'(){++callbacks;}\n'
program += '\n'.join(extract(source, name) for name in ['actionUnlocked','unlockHint','markRoutineAction','completeGameplayAction','executeAction','enterHouseSelection','enterCitySelection'])
program += r'''
byte gameCounts[13]={},favoriteGameId=0,dislikedGameId=1,careHistoryCount=0;
unsigned long lastGameBondTick=0,featuredDay=0;
byte featuredPlayedMask=0;
bool activeGameResolved=false,activeGameWon=false;
byte activeGameScore=0;
int featuredGameSlot(byte){return 0;}
void updateDevelopedHobby(){}
'''
program += extract(source,'finishGameSession')
# Compile the actual research spending method too.
program += '\nstruct ParadiseSystem {uint16_t researchPoints_=12;bool spendResearch(uint16_t);};\n'
program += extract((SRC/'ParadiseSystem.cpp').read_text(),'ParadiseSystem::spendResearch')
deep_source = (SRC/'DeepLife.cpp').read_text()
growing_source = (SRC/'GrowingTogether.cpp').read_text()
growing_header = (SRC/'GrowingTogether.h').read_text()
program += r'''
GameplayProgress &deepDailyProgress(){static GameplayProgress ledger;return ledger;}
struct DeepLifeSystem {
 uint16_t economyDay_=0,economyDayEarned_=0,economyDaySpent_=0,anniversaryDay_=1;
 uint8_t emotionMinutes_=0,emotion_=0,emotionIntensity_=0;
 uint8_t groupId_=0,groupMeetings_=0,groupLevel_=0,rivalBond_=0;
 uint8_t partnerHarmony_=50,familyMood_=50,argumentState_=0;
 uint8_t childCount_=1,childAge_[3]={};
 void configureGoals(uint16_t){} void updateSeason(uint16_t,uint8_t){}
 void generateDailyEvent(uint16_t,uint8_t,uint8_t){}
 void setEmotion(uint8_t,uint8_t,uint8_t){} void setAchievement(uint8_t){}
 void evaluateAchievements(){}
 void changeByte(uint8_t &v,int n,uint8_t bound=100){v=std::min(int(bound),int(v)+n);}
 void tick(uint32_t,uint8_t,uint8_t,uint8_t);
};
byte partnerId=255;
'''
program += extract(deep_source,'DeepLifeSystem::tick')
for name in ['GT_PROMISE_SLOTS','GT_RITUAL_COUNT','GT_TRADITION_COUNT','GT_HOBBY_COUNT']:
    program += re.search(r'constexpr uint8_t '+name+r' = [^;]+;',growing_header)[0]+'\n'
program += r'''
GameplayProgress &growingActions(){static GameplayProgress ledger;return ledger;}
GameplayProgress &growingDays(){static GameplayProgress ledger;return ledger;}
int clampInt(int v,int low,int high){return std::min(high,std::max(low,v));}
struct GrowingTogetherContext {uint32_t ageTicks=400;uint16_t ageDays=1,generation=1;uint8_t temperament=0;};
struct GrowingTogetherSystem {
 uint8_t promiseStatus_[GT_PROMISE_SLOTS]={},promiseType_[GT_PROMISE_SLOTS]={};
 uint16_t promiseDueDay_[GT_PROMISE_SLOTS]={};
 uint8_t repairProgress_=0,ruleConsistency_=0;
 int8_t relationshipMemory_[12]={};
 uint8_t ritualStreak_[GT_RITUAL_COUNT]={};uint16_t traditionMask_=0;
 uint16_t choreDoneMask_=0,choreAssignedMask_=0;
 uint8_t sharedHobby_=0,hobbyBond_[GT_HOBBY_COUNT]={},traditionStreak_[GT_TRADITION_COUNT]={};
 uint8_t schoolEffort_=0,vocabularyScore_=0;
 void changeByte(uint8_t &v,int n,uint8_t limit=100){v=std::min(int(limit),int(v)+n);}
 void recordAction(uint8_t,const GrowingTogetherContext&);
};
'''
program += extract(growing_source,'GrowingTogetherSystem::recordAction')
program += r'''
int main(){
 // Every cancelled navigation action must leave all shared systems untouched.
 for(byte a=0;a<=M_NEIGHBORS;++a){executeAction(a);}
 assert(living==0 && deep==0 && coins==0);
 for(int n:narratives)assert(n==0);
 // Failed travel/work cannot earn the high-bond coin or fulfill a promise.
 energy=0;activitySucceeds=true;executeAction(M_EXPLORE);executeAction(M_JOB);
 assert(living==0 && deep==0 && coins==0);
 energy=8;executeAction(M_EXPLORE);
 assert(living==1 && deep==1 && coins==1 && narratives[GTA_EXPLORE]==1);
 executeAction(M_EXPLORE);
 assert(living==1 && deep==1 && coins==1);
 ++ageTicks;executeAction(M_EXPLORE);
 assert(living==2 && deep==2 && coins==2);
 // Completing an action and marking routine flags cannot double narrative progress.
 executeAction(M_GARDEN);markRoutineAction(ROUTINE_PLAY);markRoutineAction(ROUTINE_PLAY);
 assert(narratives[GTA_GARDEN]==1 && narratives[GTA_PLAY]==0);
 // Both direct world routes use exactly the same sitter/age/sleep checks.
 int old=callbacks;sitterTicksRemaining=3;enterHouseSelection();enterCitySelection();
 assert(callbacks==old);assert(!completeGameplayAction(M_STUDY));
 sitterTicksRemaining=0;sleeping=true;enterHouseSelection();assert(callbacks==old);
 sleeping=false;stage=STAGE_BABY;enterHouseSelection();assert(callbacks==old);
 stage=STAGE_ADULT;
 // A cancelled arcade game earns nothing even after the old hold-exit threshold.
 int outcomes=resultCallbacks;int count=gameCounts[0];
 activeGameResolved=false;finishGameSession(0);
 assert(gameCounts[0]==count && resultCallbacks==outcomes);
 // Genuine losses count as completed play; a result can only be consumed once.
 activeGameResolved=true;activeGameWon=false;activeGameScore=2;finishGameSession(0);
 assert(gameCounts[0]==count+1 && resultCallbacks==outcomes+2 && lastResult==0);
 finishGameSession(0);assert(gameCounts[0]==count+1 && resultCallbacks==outcomes+2);
 activeGameResolved=true;activeGameWon=true;finishGameSession(1);assert(lastResult==1);
 // New life/generation does not inherit a spent transient activity token.
 GameplayProgress ledger;assert(ledger.claim(63,1,1));assert(!ledger.claim(63,1,1));
 assert(!ledger.claim(64,1,1));assert(ledger.claim(63,2,1));assert(ledger.claim(63,2,2));
 ledger.reset();assert(ledger.claim(63,2,2));
 assert(!GameplayProgress::familyReady(3,100));assert(!GameplayProgress::familyReady(4,79));
 assert(GameplayProgress::familyReady(4,80));
 // Talents survive reload from the same seed and vary among same-lineage pets.
 auto talent=GameplayProgress::interest(2,3,100);
 assert(talent==GameplayProgress::interest(2,3,100));
 assert(talent!=GameplayProgress::interest(2,3,101));
 // Insufficient research fails atomically; exact payment is possible.
 ParadiseSystem research;assert(!research.spendResearch(13));assert(research.researchPoints_==12);
 assert(research.spendResearch(12));assert(research.researchPoints_==0);
 // Hour-based events run once on their event day, not twelve times an hour.
 DeepLifeSystem simulation;
 for(int i=0;i<12;++i)simulation.tick(6,16,2,0);
 assert(simulation.groupMeetings_==1 && simulation.groupLevel_==1);
 for(int i=0;i<12;++i)simulation.tick(6,7,2,0);
 assert(simulation.childAge_[0]==1);
 partnerId=0;
 for(int hour=0;hour<24;++hour)for(int i=0;i<12;++i)simulation.tick(31,hour,4,0);
 assert(simulation.partnerHarmony_==55);
 // Actual promises require the matching completed action; garden cannot do cleaning.
 GrowingTogetherSystem story;GrowingTogetherContext context;
 story.choreAssignedMask_=(1U<<0)|(1U<<1);
 story.promiseStatus_[0]=1;story.promiseType_[0]=GTA_GARDEN;story.promiseDueDay_[0]=2;
 story.hobbyBond_[1]=10;story.recordAction(GTA_GARDEN,context);
 assert(story.promiseStatus_[0]==2 && story.choreDoneMask_==(1U<<1));
 assert(story.hobbyBond_[1]==12 && story.traditionStreak_[1]==1);
 story.recordAction(GTA_GARDEN,context);assert(story.hobbyBond_[1]==12);
 ++context.ageTicks;story.recordAction(GTA_GARDEN,context);
 assert(story.hobbyBond_[1]==14 && story.traditionStreak_[1]==1);
 ++context.ageTicks;++context.ageDays;story.recordAction(GTA_GARDEN,context);
 assert(story.traditionStreak_[1]==2);
 std::cout<<"Gameplay regression checks passed\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'checks.cpp';cpp.write_text(program)
    exe=Path(tmp)/'checks'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(SRC),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
