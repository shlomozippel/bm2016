int rainbow() {
    static uint8_t hue = 0;
    fill_rainbow( leds, NUM_LEDS, hue, 7);    
    hue++;
    return 0;
}

int boa_rainbow() {
    // kinda hacky, assumes boa has 64 LEDs with 2 strips of 32 folded over
    static uint8_t hue = 0;
    fill_rainbow( leds, 32, hue, 7);    
    for (int i=0; i<32; i++) {
      leds[63-i] = leds[i];
    }
    hue++;
    return 0;
}

int confetti() {
    static uint8_t hue = 0;
    
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy( leds, NUM_LEDS, 7);
    if (random8() > 100) {
      int pos = random8(NUM_LEDS);
      leds[pos] += CHSV( hue + random8(64), 200, 255);
    }

    hue++;
    return 0;
}

int sinelon() {
    static uint8_t hue = 0;

    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy( leds, NUM_LEDS, 20);
    int pos = beatsin16(11,0,NUM_LEDS);
    leds[pos] |= CHSV( hue, 255, 255);
    pos = beatsin16(13,0,NUM_LEDS);
    leds[pos] |= CHSV( hue + 128, 255, 255);
    
    hue++;
    return 0;
} 

