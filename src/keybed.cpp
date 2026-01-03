
/**
 * Class to poll the keybed shift registers and return an array of pressed key
 */
class Keybed : public AudioStream {
  public:
    Vibrato() : AudioStream(1, inputQueueArray) {
    }