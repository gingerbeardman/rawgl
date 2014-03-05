
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "mixer.h"
#include "systemstub.h"


static int8_t addclamp(int a, int b) {
	int add = a + b;
	if (add < -128) {
		add = -128;
	}
	else if (add > 127) {
		add = 127;
	}
	return (int8_t)add;
}

Mixer::Mixer(SystemStub *stub) 
	: _stub(stub) {
}

void Mixer::init() {
	memset(_channels, 0, sizeof(_channels));
	_mutex = _stub->createMutex();
	_stub->startAudio(Mixer::mixCallback, this);
}

void Mixer::free() {
	stopAll();
	_stub->stopAudio();
	_stub->destroyMutex(_mutex);
}

void Mixer::playChannel(uint8_t channel, const MixerChunk *mc, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
	assert(channel < NUM_CHANNELS);
	MutexStack(_stub, _mutex);
	MixerChannel *ch = &_channels[channel];
	ch->active = true;
	ch->volume = volume;
	ch->chunk = *mc;
	ch->chunkPos = 0;
	ch->chunkInc = (freq << 8) / _stub->getOutputSampleRate();
}

void Mixer::stopChannel(uint8_t channel) {
	debug(DBG_SND, "Mixer::stopChannel(%d)", channel);
	assert(channel < NUM_CHANNELS);
	MutexStack(_stub, _mutex);	
	_channels[channel].active = false;
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
	debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
	assert(channel < NUM_CHANNELS);
	MutexStack(_stub, _mutex);
	_channels[channel].volume = volume;
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	MutexStack(_stub, _mutex);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;		
	}
}

void Mixer::mix(int8_t *buf, int len) {
	MutexStack(_stub, _mutex);
	memset(buf, 0, len);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *ch = &_channels[i];
		if (ch->active) {
			int8_t *pBuf = buf;
			for (int j = 0; j < len; ++j, ++pBuf) {
				uint16_t p1, p2;
				uint16_t ilc = (ch->chunkPos & 0xFF);
				p1 = ch->chunkPos >> 8;
				ch->chunkPos += ch->chunkInc;
				if (ch->chunk.loopLen != 0) {
					if (p1 == ch->chunk.loopPos + ch->chunk.loopLen - 1) {
						debug(DBG_SND, "Looping sample on channel %d", i);
						ch->chunkPos = p2 = ch->chunk.loopPos;
					} else {
						p2 = p1 + 1;
					}
				} else {
					if (p1 == ch->chunk.len - 1) {
						debug(DBG_SND, "Stopping sample on channel %d", i);
						ch->active = false;
						break;
					} else {
						p2 = p1 + 1;
					}
				}
				// interpolate
				int8_t b1 = *(int8_t *)(ch->chunk.data + p1);
				int8_t b2 = *(int8_t *)(ch->chunk.data + p2);
				int8_t b = (int8_t)((b1 * (0xFF - ilc) + b2 * ilc) >> 8);
				// set volume and clamp
				*pBuf = addclamp(*pBuf, (int)b * ch->volume / 0x40);
			}
		}
	}
}

void Mixer::mixCallback(void *param, uint8_t *buf, int len) {
	((Mixer *)param)->mix((int8_t *)buf, len);
}