#include <snes.hpp>

#define MSU1_CPP
namespace SNES {

MSU1 msu1;

#include "serialization.cpp"

void MSU1::Enter() { msu1.enter(); }

void MSU1::enter() {
  if(boot == true) {
    boot = false;
    for(unsigned addr = 0x2000; addr <= 0x2007; addr++) mmio_write(addr, 0x00);
  }

  while(true) {
    if(scheduler.sync == Scheduler::SynchronizeMode::All) {
      scheduler.exit(Scheduler::ExitReason::SynchronizeEvent);
    }

    int16 left = 0, right = 0;

    if(mmio.audio_play) {
      if(audiofile.open()) {
        if(audiofile.end()) {
          if(!mmio.audio_repeat) {
            mmio.audio_play = false;
            audiofile.seek(mmio.audio_offset = 8);
          } else {
            audiofile.seek(mmio.audio_offset = mmio.audio_loop_offset);
          }
        } else {
          mmio.audio_offset += 4;
          left  = audiofile.readl(2);
          right = audiofile.readl(2);
        }
      } else {
        mmio.audio_play = false;
      }
    }

    signed lchannel = (double)left  * (double)mmio.audio_volume / 255.0;
    signed rchannel = (double)right * (double)mmio.audio_volume / 255.0;
    left  = sclamp<16>(lchannel);
    right = sclamp<16>(rchannel);
    if(dsp.mute()) left = 0, right = 0;

    audio.coprocessor_sample(left, right);
    step(1);
    synchronize_cpu();
  }
}

void MSU1::init() {
}

void MSU1::enable() {
  audio.coprocessor_enable(true);
  audio.coprocessor_frequency(44100.0);

  if(datafile.open()) datafile.close();
  datafile.open(string(cartridge.basename(), ".msu"), file::mode::read);
}

void MSU1::power() {
  reset();
}

void MSU1::reset() {
  create(MSU1::Enter, 44100);
  boot = true;

  mmio.data_offset  = 0;
  mmio.audio_offset = 0;
  mmio.audio_track  = 0;
  mmio.audio_volume = 255;
  mmio.data_busy    = true;
  mmio.audio_busy   = true;
  mmio.audio_repeat = false;
  mmio.audio_play   = false;
  mmio.audio_error  = false;
}

uint8 MSU1::mmio_read(unsigned addr) {
  if(addr == 0x2000) {
    return (mmio.data_busy    << 7)
         | (mmio.audio_busy   << 6)
         | (mmio.audio_repeat << 5)
         | (mmio.audio_play   << 4)
         | (mmio.audio_error  << 3)
         | (Revision          << 0);
  }

  if(addr == 0x2001) {
    if(mmio.data_busy) return 0x00;
    mmio.data_offset++;
    if(datafile.open()) return datafile.read();
    return 0x00;
  }

  if(addr == 0x2002) return 'S';
  if(addr == 0x2003) return '-';
  if(addr == 0x2004) return 'M';
  if(addr == 0x2005) return 'S';
  if(addr == 0x2006) return 'U';
  if(addr == 0x2007) return '0' + Revision;

  return 0x00;
}

void MSU1::mmio_write(unsigned addr, uint8 data) {
  if(addr == 0x2000) {
    mmio.data_offset = (mmio.data_offset & 0xffffff00) | (data <<  0);
  }

  if(addr == 0x2001) {
    mmio.data_offset = (mmio.data_offset & 0xffff00ff) | (data <<  8);
  }

  if(addr == 0x2002) {
    mmio.data_offset = (mmio.data_offset & 0xff00ffff) | (data << 16);
  }

  if(addr == 0x2003) {
    mmio.data_offset = (mmio.data_offset & 0x00ffffff) | (data << 24);
    if(datafile.open()) datafile.seek(mmio.data_offset);
    mmio.data_busy = false;
  }

  if(addr == 0x2004) {
    mmio.audio_track = (mmio.audio_track & 0xff00) | (data << 0);
  }

  if(addr == 0x2005) {
    mmio.audio_track = (mmio.audio_track & 0x00ff) | (data << 8);
    if(audiofile.open()) audiofile.close();
    if(audiofile.open(string(cartridge.basename(), "-", mmio.audio_track, ".pcm"), file::mode::read)) {
      uint32 header = audiofile.readm(4);
      if(header != 0x4d535531) {  //verify 'MSU1' header
        audiofile.close();
      } else {
        mmio.audio_offset = 8;
        mmio.audio_loop_offset = 8 + audiofile.readl(4) * 4;
      }
    }
    mmio.audio_busy   = false;
    mmio.audio_repeat = false;
    mmio.audio_play   = false;
    mmio.audio_error  = !audiofile.open();
  }

  if(addr == 0x2006) {
    mmio.audio_volume = data;
  }

  if(addr == 0x2007) {
    mmio.audio_repeat = data & 2;
    mmio.audio_play   = data & 1;
  }
}

}
