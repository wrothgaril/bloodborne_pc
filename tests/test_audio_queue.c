/* Exercise the production mixer/queue without a physical playback device. */
#include "../src/runtime_audio.c"
#include <assert.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name; return 0;
}
/* 1.9.4's mute in the background (gpu/bbgpu.h): the window has the focus here. */
int bbgpu_audio_muted(void) { return 0; }
#ifdef _WIN32
void compat_sleep_ns(uint64_t ns) { (void)ns; }
#endif

int main(void) {
    Port *p=&ports[0];
    p->used=1; p->frames=256; p->channels=p->out_channels=2; p->sample_bytes=2;
    pthread_mutex_init(&p->lock,NULL);
    for (int i=0;i<8;++i) p->volume[i]=VOLUME_0DB;
    device_period_frames=1024;
    assert(queue_low_bytes(p)==4096*4);
    device_period_frames=0;
    assert(queue_low_bytes(p)==3840*4);
    device_period_frames=16384;
    assert(queue_low_bytes(p)==9600*4);
    device_period_frames=1024;
    assert(SDL_Init(SDL_INIT_AUDIO));
    SDL_AudioSpec spec={SDL_AUDIO_S16,2,48000};
    p->stream=SDL_CreateAudioStream(&spec,&spec);
    assert(p->stream);
    int16_t samples[512];
    for (int i=0;i<512;++i) samples[i]=1000;
    assert(output_port(0x20000000,samples,0)==0);
    assert(SDL_GetAudioStreamQueued(p->stream)==queue_low_bytes(p)+(int)sizeof(samples));
    assert(p->primed);
    /* Overfull queues discard the new packet, preserving all buffered audio. */
    put_silence(p->stream,queue_low_bytes(p));
    int queued=SDL_GetAudioStreamQueued(p->stream);
    assert(output_port(0x20000000,samples,0)==0);
    assert(SDL_GetAudioStreamQueued(p->stream)==queued);
    assert(p->drop_warned);
    SDL_ClearAudioStream(p->stream); /* Simulate an actual underrun. */
    assert(output_port(0x20000000,samples,0)==0);
    assert(SDL_GetAudioStreamQueued(p->stream)==(1024+256)*4);
    assert(!p->drop_warned);
    int16_t output[(1024+256)*2];
    assert(SDL_GetAudioStreamData(p->stream,output,sizeof(output))==(int)sizeof(output));
    for (int i=0;i<1024*2;++i) assert(output[i]==0);
    for (int i=1024*2;i<(1024+256)*2;++i) assert(output[i]==1000);
    SDL_DestroyAudioStream(p->stream);
    p->stream=NULL;
    pthread_mutex_destroy(&p->lock);
    SDL_Quit();
    puts("Audio queue: bounded lead, initial prefill, drop without flush, underrun refill and PCM PASS");
    return 0;
}
