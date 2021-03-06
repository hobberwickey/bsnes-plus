#include <snes.hpp>

#define SUPERGAMEBOY_CPP
namespace SNES {

SuperGameBoy supergameboy;

#include "serialization.cpp"

void SuperGameBoy::Enter() { supergameboy.enter(); }

void SuperGameBoy::enter() {
  if(!sgb_run) while(true) {
    if(scheduler.sync == Scheduler::SynchronizeMode::All) {
      scheduler.exit(Scheduler::ExitReason::SynchronizeEvent);
    }

    audio.coprocessor_sample(0, 0);
    step(1);
    synchronize_cpu();
  }

  while(true) {
    if(scheduler.sync == Scheduler::SynchronizeMode::All) {
      scheduler.exit(Scheduler::ExitReason::SynchronizeEvent);
    }

    unsigned samples = sgb_run(samplebuffer, 16);
    for(unsigned i = 0; i < samples; i++) {
      int16 left  = samplebuffer[i] >>  0;
      int16 right = samplebuffer[i] >> 16;

      //SNES audio is notoriously quiet; lower Game Boy samples to match SGB sound effects
      audio.coprocessor_sample(left / 3, right / 3);
    }

    step(samples);
    synchronize_cpu();
  }
}

void SuperGameBoy::save() {
  if(sgb_save) sgb_save();
}

uint8 SuperGameBoy::mmio_read(unsigned addr) {
  if(addr == 0x2181) return mmio[0]->mmio_read(addr);
  if(addr == 0x2182) return mmio[1]->mmio_read(addr);
  if(addr == 0x420b) return mmio[2]->mmio_read(addr);

  return 0x00;
}

void SuperGameBoy::mmio_write(unsigned addr, uint8 data) {
  if(addr == 0x2181) {
    row = (row & 0xff00) | (data << 0);
    mmio[0]->mmio_write(addr, data);
  }

  if(addr == 0x2182) {
    row = (row & 0x00ff) | (data << 8);
    mmio[1]->mmio_write(addr, data);
  }

  if(addr == 0x420b) {
    if(data == 0x10 && sgb_row) {
      if(row >= 0x5000 && row <= 0x6540) sgb_row((row - 0x5000) / 320);
      if(row >= 0x6800 && row <= 0x7d40) sgb_row((row - 0x6800) / 320);
    }
    mmio[2]->mmio_write(addr, data);
  }
}

uint8 SuperGameBoy::read(unsigned addr) {
  if(sgb_read && !debugger_access()) return sgb_read(addr);
  return 0x00;
}

void SuperGameBoy::write(unsigned addr, uint8 data) {
  if(sgb_write && !debugger_access()) sgb_write(addr, data);
}

void SuperGameBoy::init() {
  if(open("supergameboy")) {
    sgb_rom       = sym("sgb_rom");
    sgb_ram       = sym("sgb_ram");
    sgb_rtc       = sym("sgb_rtc");
    sgb_init      = sym("sgb_init");
    sgb_term      = sym("sgb_term");
    sgb_power     = sym("sgb_power");
    sgb_reset     = sym("sgb_reset");
    sgb_row       = sym("sgb_row");
    sgb_read      = sym("sgb_read");
    sgb_write     = sym("sgb_write");
    sgb_run       = sym("sgb_run");
    sgb_save      = sym("sgb_save");
    sgb_serialize = sym("sgb_serialize");
  }
}

void SuperGameBoy::enable() {
  mmio[0] = memory::mmio.handle(0x2181);
  mmio[1] = memory::mmio.handle(0x2182);
  mmio[2] = memory::mmio.handle(0x420b);
  memory::mmio.map(0x2181, *this);
  memory::mmio.map(0x2182, *this);
  memory::mmio.map(0x420b, *this);
}

void SuperGameBoy::power() {
  unsigned frequency = (cartridge.supergameboy_version() == Cartridge::SuperGameBoyVersion::Version1 ? system.cpu_frequency() / 10 : 2097152);
  create(SuperGameBoy::Enter, frequency);

  audio.coprocessor_enable(true);
  audio.coprocessor_frequency(cartridge.supergameboy_version() == Cartridge::SuperGameBoyVersion::Version1 ? 2147727.0 : 2097152.0);

  sgb_rom(memory::gbrom.data(), memory::gbrom.size());
  sgb_ram(memory::gbram.data(), memory::gbram.size());
  sgb_rtc(memory::gbrtc.data(), memory::gbrtc.size());

  bool version = (cartridge.supergameboy_version() == Cartridge::SuperGameBoyVersion::Version1) ? 0 : 1;
  if(sgb_init) sgb_init(version);
  if(sgb_power) sgb_power();
}

void SuperGameBoy::reset() {
  unsigned frequency = (cartridge.supergameboy_version() == Cartridge::SuperGameBoyVersion::Version1 ? system.cpu_frequency() / 10 : 2097152);
  create(SuperGameBoy::Enter, frequency);

  if(sgb_reset) sgb_reset();
}

void SuperGameBoy::unload() {
  if(sgb_term) sgb_term();
}

}
