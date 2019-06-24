#ifndef ___NODE__H___
#define ___NODE__H___

#include <string.h>
#include <stdint.h>
#include <Arduino.h>
#include <Servo.h>
#include <NadaMQ.h>
#include <CArrayDefs.h>
#include "RPCBuffer.h"  // Define packet sizes
#include "DropbotDx/Properties.h"  // Define package name, URL, etc.
#include <BaseNodeRpc/BaseNode.h>
#include <BaseNodeRpc/BaseNodeEeprom.h>
#include <BaseNodeRpc/BaseNodeI2c.h>
#include <BaseNodeRpc/BaseNodeConfig.h>
#include <BaseNodeRpc/BaseNodeState.h>
#include <BaseNodeRpc/BaseNodeI2cHandler.h>
#include <BaseNodeRpc/BaseNodeSerialHandler.h>
#include <BaseNodeRpc/SerialHandler.h>
#include <ADC.h>
#include <RingBufferDMA.h>
#include <DMAChannel.h>
#include <TeensyMinimalRpc/ADC.h>  // Analog to digital converter
#include <TeensyMinimalRpc/DMA.h>  // Direct Memory Access
#include <TeensyMinimalRpc/SIM.h>  // System integration module (clock gating)
#include <TeensyMinimalRpc/PIT.h>  // Programmable interrupt timer
#include <TeensyMinimalRpc/aligned_alloc.h>
#include <pb_cpp_api.h>
#include <pb_validate.h>
#include <pb_eeprom.h>
#include <LinkedList.h>
#include <TimerOne.h>
#include "dropbot_dx_config_validate.h"
#include "dropbot_dx_state_validate.h"
#include "DropbotDx/config_pb.h"
#include "DropbotDx/state_pb.h"


const uint32_t ADC_BUFFER_SIZE = 4096;

extern void dma_ch0_isr(void);
extern void dma_ch1_isr(void);
extern void dma_ch2_isr(void);
extern void dma_ch3_isr(void);
extern void dma_ch4_isr(void);
extern void dma_ch5_isr(void);
extern void dma_ch6_isr(void);
extern void dma_ch7_isr(void);
extern void dma_ch8_isr(void);
extern void dma_ch9_isr(void);
extern void dma_ch10_isr(void);
extern void dma_ch11_isr(void);
extern void dma_ch12_isr(void);
extern void dma_ch13_isr(void);
extern void dma_ch14_isr(void);
extern void dma_ch15_isr(void);

namespace dropbot_dx {

// Define the array that holds the conversions here.
// buffer_size must be a power of two.
// The buffer is stored with the correct alignment in the DMAMEM section
// the +0 in the aligned attribute is necessary b/c of a bug in gcc.
DMAMEM static volatile int16_t __attribute__((aligned(ADC_BUFFER_SIZE+0))) adc_buffer[ADC_BUFFER_SIZE];


const size_t FRAME_SIZE = (3 * sizeof(uint8_t)  // Frame boundary
                           - sizeof(uint16_t)  // UUID
                           - sizeof(uint16_t)  // Payload length
                           - sizeof(uint16_t));  // CRC

class Node;
const char HARDWARE_VERSION_[] = "0.3";

typedef nanopb::EepromMessage<dropbot_dx_Config,
                              config_validate::Validator<Node> > config_t;
typedef nanopb::Message<dropbot_dx_State,
                        state_validate::Validator<Node> > state_t;

class Node :
  public BaseNode,
  public BaseNodeEeprom,
  public BaseNodeI2c,
  public BaseNodeConfig<config_t>,
  public BaseNodeState<state_t>,
#ifndef DISABLE_SERIAL
  public BaseNodeSerialHandler,
#endif  // #ifndef DISABLE_SERIAL
  public BaseNodeI2cHandler<base_node_rpc::i2c_handler_t> {
public:
  typedef PacketParser<FixedPacket> parser_t;

  static void timer_callback();
  Servo servo_;

  static const uint32_t BUFFER_SIZE = 8192;  // >= longest property string

  static const uint16_t MAX_NUMBER_OF_CHANNELS = 120;

  static const uint8_t HIGH_PIN = 6;
  static const uint8_t LOW_PIN = 7;
  static const uint8_t LIGHT_PIN = 5;

  // pins connected to the boost converter
  static const uint8_t MCP41050_CS_PIN = 10;
  static const uint8_t SHDN_PIN = 4;

  static const uint8_t HV_OUTPUT_SELECT_PIN = 8;

  // SPI pins
  static const uint8_t SCK_PIN = 13;
  static const uint8_t MOSI_PIN = 11;

  // PCA9505 (gpio) chip/register addresses
  static const uint8_t PCA9505_CONFIG_IO_REGISTER = 0x18;
  static const uint8_t PCA9505_OUTPUT_PORT_REGISTER = 0x08;

  // use dma with ADC0
  RingBufferDMA *dmaBuffer_;

  static const float R6;

  uint8_t buffer_[BUFFER_SIZE];
  uint8_t state_of_channels_[MAX_NUMBER_OF_CHANNELS / 8];
  uint16_t number_of_channels_;

  ADC *adc_;
  uint32_t adc_period_us_;
  uint32_t adc_timestamp_us_;
  bool adc_tick_tock_;
  uint32_t adc_millis_;
  uint32_t adc_SYST_CVR_;
  uint32_t adc_millis_prev_;
  uint32_t adc_SYST_CVR_prev_;
  uint32_t adc_count_;
  int8_t dma_channel_done_;
  int8_t last_dma_channel_done_;
  bool adc_read_active_;
  LinkedList<uint32_t> allocations_;
  LinkedList<uint32_t> aligned_allocations_;

  Node() : BaseNode(),
           BaseNodeConfig<config_t>(dropbot_dx_Config_fields),
           BaseNodeState<state_t>(dropbot_dx_State_fields), dmaBuffer_(NULL),
           adc_period_us_(0), adc_timestamp_us_(0), adc_tick_tock_(false),
           adc_count_(0), dma_channel_done_(-1), last_dma_channel_done_(-1),
           adc_read_active_(false) {
    pinMode(LED_BUILTIN, OUTPUT);
  }

  UInt8Array get_buffer() { return UInt8Array_init(sizeof(buffer_), buffer_); }
  /* This is a required method to provide a temporary buffer to the
   * `BaseNode...` classes. */

  void begin();
  /****************************************************************************
   * # User-defined methods #
   *
   * Add new methods below.  When Python package is generated using the
   * command, `paver sdist` from the project root directory, the signatures of
   * the methods below will be scanned and code will automatically be generated
   * to support calling the methods from Python over a serial connection.
   *
   * e.g.
   *
   *     bool less_than(float a, float b) { return a < b; }
   *
   * See [`arduino_rpc`][1] and [`base_node_rpc`][2] for more details.
   *
   * [1]: https://github.com/wheeler-microfluidics/arduino_rpc
   * [2]: https://github.com/wheeler-microfluidics/base_node_rpc
   */
  uint8_t servo_read() { return servo_.read(); }
  void servo_write(uint8_t angle) { servo_.write(angle); }
  void servo_write_microseconds(uint16_t us) { servo_.writeMicroseconds(us); }
  bool servo_attached() { return servo_.attached(); }

  uint16_t number_of_channels() const { return number_of_channels_; }
  void set_number_of_channels(uint16_t number_of_channels) { number_of_channels_ = number_of_channels; }
  UInt8Array hardware_version() { return UInt8Array_init(strlen(HARDWARE_VERSION_),
                      (uint8_t *)&HARDWARE_VERSION_[0]); }
  UInt8Array _uuid() {
    /* Read unique chip identifier. */
    UInt8Array result = get_buffer();
    result.length = 4 * sizeof(uint32_t);
    memcpy(&result.data[0], &SIM_UIDH, result.length);
    return result;
  }

  UInt8Array state_of_channels() {
    for (uint8_t chip = 0; chip < number_of_channels_ / 40; chip++) {
      for (uint8_t port = 0; port < 5; port++) {
        Wire.beginTransmission((uint8_t)config_._.switching_board_i2c_address + chip);
        Wire.write(PCA9505_OUTPUT_PORT_REGISTER + port);
        Wire.endTransmission();

        delayMicroseconds(100); // needed when using Teensy

        Wire.requestFrom(config_._.switching_board_i2c_address + chip, 1);
        if (Wire.available()) {
          state_of_channels_[chip*5 + port] = ~Wire.read();
        } else {
          return UInt8Array_init_default();
        }
      }
    }
    return UInt8Array_init(number_of_channels_ / 8,
                      (uint8_t *)&state_of_channels_[0]);
  }

  bool set_id(UInt8Array id) {
    if (id.length > sizeof(config_._.id) - 1) {
      return false;
    }
    memcpy(config_._.id, &id.data[0], id.length);
    config_._.id[id.length] = 0;
    config_._.has_id = true;
    config_.save();
    return true;
  }

  bool set_state_of_channels(UInt8Array channel_states) {
    if (channel_states.length == number_of_channels_ / 8) {
      for (uint16_t i = 0; i < channel_states.length; i++) {
        state_of_channels_[i] = channel_states.data[i];
      }
      // Each PCA9505 chip has 5 8-bit output registers for a total of 40 outputs
      // per chip. We can have up to 8 of these chips on an I2C bus, which means
      // we can control up to 320 channels.
      //   Each register represent 8 channels (i.e. the first register on the
      // first PCA9505 chip stores the state of channels 0-7, the second register
      // represents channels 8-15, etc.).
      for (uint8_t chip = 0; chip < number_of_channels_ / 40; chip++) {
        for (uint8_t port = 0; port < 5; port++) {
          buffer_[0] = PCA9505_OUTPUT_PORT_REGISTER + port;
          buffer_[1] = ~state_of_channels_[chip*5 + port];
          i2c_write(config_._.switching_board_i2c_address + chip,
                    UInt8Array_init(2, (uint8_t *)&buffer_[0]));
          delayMicroseconds(200); // this delay is necessary if we are operating with a 400kbps i2c clock
        }
      }
      return true;
    }
    return false;
  }

  bool on_state_frequency_changed(float frequency) {
    /* This method is triggered whenever a frequency is included in a state
     * update. */
    if ((config_._.min_frequency <= frequency) &&
                (frequency <= config_._.max_frequency)) {
      if (frequency == 0) { // DC mode
        digitalWrite(Node::HIGH_PIN, HIGH); // set not blanked pin high
        digitalWrite(Node::LOW_PIN, LOW); // set not blanked pin high
        Timer1.stop(); // stop timer
      } else {
        Timer1.setPeriod(500000.0 / frequency); // set timer period in ms
        Timer1.restart();
      }
      return true;
    }
    return false;
  }

  float min_waveform_voltage() {
    return 1.5 / 2.0 * (R6 / (config_._.pot_max + config_._.R7) + 1);
  }

  bool _set_voltage(float voltage) {
    float value = R6 / ( 2 * voltage / 1.5 - 1 ) - config_._.R7;
    if ( voltage <= config_._.max_voltage && value <= config_._.pot_max && value >= 0 ) {
      // This method is triggered whenever a voltage is included in a state
      // update.

      // take the SS pin low to select the chip:
      digitalWrite(MCP41050_CS_PIN, LOW);

      // send Command to write value and enable the pot
      shiftOut(MOSI_PIN, SCK_PIN, MSBFIRST, 0x1F);
      // send in the value via SPI:
      shiftOut(MOSI_PIN, SCK_PIN, MSBFIRST, 255 - value / config_._.pot_max * 255);

      // take the SS pin high to de-select the chip:
      digitalWrite(MCP41050_CS_PIN, HIGH);
      return true;
    }
    return false;
  }

  bool on_state_voltage_changed(float voltage) {
    return _set_voltage(voltage);
  }

  bool on_state_hv_output_enabled_changed(bool value) {
    if (value) {
      // If we're turning the output on, we need to start with a low voltage,
      // enable the MAX1771, then increase the voltage. Otherwise, if the voltage
      // is > ~100 the MAX1771 will not turn on.
      _set_voltage(15);
      delay(100);
      digitalWrite(SHDN_PIN, !value);
      delay(100);
      _set_voltage(state_._.voltage);
      Timer1.setPeriod(500000.0 / state_._.frequency); // set timer period in ms
      Timer1.restart();
    } else {
      digitalWrite(SHDN_PIN, !value);
      Timer1.stop(); // stop timer
    }
    return true;
  }

  bool on_state_hv_output_selected_changed(bool value) {
    digitalWrite(HV_OUTPUT_SELECT_PIN, !value);
    return true;
  }

  bool on_state_magnet_engaged_changed(bool value) {
    if (value) {
      _magnet_engage();
    } else {
      _magnet_disengage();
    }
    return true;
  }

  bool on_config_servo_pin_changed(uint32_t value) {
    servo_.attach(value);
    return true;
  }

  bool on_config_light_intensity_changed(float value) {
    if (state_._.light_enabled) {
      analogWrite(LIGHT_PIN, value * 255.0);
    }
    return true;
  }

  bool on_state_light_enabled_changed(bool value) {
    if (value) {
      analogWrite(LIGHT_PIN, config_._.light_intensity * 255.0);
    } else {
      analogWrite(LIGHT_PIN, 0);
    }
    return true;
  }

  bool magnet_engaged() { return state_._.magnet_engaged; }

  // Local methods
  // TODO: Should likely be private, but need to add private handling to code
  // scraper/generator.
  void _initialize_switching_boards();
  void _magnet_engage() { servo_.write(config_._.engaged_angle); }
  void _magnet_disengage() { servo_.write(config_._.disengaged_angle); }

  float test(float a) { return 2 * a; }

  /////////////// METHODS TO SET/GET SETTINGS OF THE ADC ////////////////////

  void on_tick() {
    if (adc_read_active_) return;
    uint8_t channel;
    switch(adc_count_ & 0x01) {
      case(0): channel = A0; break;
      case(1): channel = A1; break;
      //case(2): channel = A2;
      //case(3): channel = A3;
      default: channel = A0;
    }
    adc_->startSingleRead(channel, ADC_0);
  }

  uint32_t V__SYST_CVR() { return SYST_CVR; }
  uint32_t V__SCB_ICSR() { return SCB_ICSR; }
  uint32_t D__F_CPU() { return F_CPU; }
  uint32_t D__F_BUS() { return F_BUS; }

  void on_adc_done() {
    if (adc_read_active_) return;
    adc_count_++;
    //adc_tick_tock_ = !adc_tick_tock_;
    //digitalWriteFast(LED_BUILTIN, adc_tick_tock_);
    //adc_SYST_CVR_prev_ = adc_SYST_CVR_;
    //adc_millis_ = millis();
    //adc_SYST_CVR_ = SYST_CVR;
  }

  float adc_timestamp_us() const {
    return compute_timestamp_us(adc_SYST_CVR_, adc_millis_);
  }
  float compute_timestamp_us(uint32_t _SYST_CVR, uint32_t _millis) const {
    uint32_t current = ((F_CPU / 1000) - 1) - _SYST_CVR;
#if defined(KINETISL) && F_CPU == 48000000
    return _millis * 1000 + ((current * (uint32_t)87381) >> 22);
#elif defined(KINETISL) && F_CPU == 24000000
    return _millis * 1000 + ((current * (uint32_t)174763) >> 22);
#endif
    return 1000 * (_millis + current * (1000. / F_CPU));
  }

  float adc_period_us() const {
    uint32_t _SYST_CVR = ((adc_SYST_CVR_ < adc_SYST_CVR_prev_)
                          ? adc_SYST_CVR_ + 1000
                          : adc_SYST_CVR_);
    return (compute_timestamp_us(_SYST_CVR, 0) -
            compute_timestamp_us(adc_SYST_CVR_prev_, 0));
  }

  //! Change the resolution of the measurement.
  /*
  *  \param bits is the number of bits of resolution.
  *  For single-ended measurements: 8, 10, 12 or 16 bits.
  *  For differential measurements: 9, 11, 13 or 16 bits.
  *  If you want something in between (11 bits single-ended for example) select the inmediate higher
  *  and shift the result one to the right.
  *
  *  Whenever you change the resolution, change also the comparison values (if you use them).
  */
  void setResolution(uint8_t bits, int8_t adc_num) {
    adc_->setResolution(bits, adc_num);
  }

  //! Returns the resolution of the ADC_Module.
  uint8_t getResolution(int8_t adc_num) {
    return adc_->getResolution(adc_num);
  }

  //! Returns the maximum value for a measurement: 2^res-1.
  uint32_t getMaxValue(int8_t adc_num) {
    return adc_->getMaxValue(adc_num);
  }

  void setConversionSpeed(uint8_t speed, int8_t adc_num) {
    //! Sets the conversion speed (changes the ADC clock, ADCK)
    /**
     * \param speed can be ADC_VERY_LOW_SPEED, ADC_LOW_SPEED, ADC_MED_SPEED, ADC_HIGH_SPEED_16BITS, ADC_HIGH_SPEED or ADC_VERY_HIGH_SPEED.
     *
     * ADC_VERY_LOW_SPEED is guaranteed to be the lowest possible speed within specs for resolutions less than 16 bits (higher than 1 MHz),
     * it's different from ADC_LOW_SPEED only for 24, 4 or 2 MHz bus frequency.
     * ADC_LOW_SPEED is guaranteed to be the lowest possible speed within specs for all resolutions (higher than 2 MHz).
     * ADC_MED_SPEED is always >= ADC_LOW_SPEED and <= ADC_HIGH_SPEED.
     * ADC_HIGH_SPEED_16BITS is guaranteed to be the highest possible speed within specs for all resolutions (lower or eq than 12 MHz).
     * ADC_HIGH_SPEED is guaranteed to be the highest possible speed within specs for resolutions less than 16 bits (lower or eq than 18 MHz).
     * ADC_VERY_HIGH_SPEED may be out of specs, it's different from ADC_HIGH_SPEED only for 48, 40 or 24 MHz bus frequency.
     *
     * Additionally the conversion speed can also be ADC_ADACK_2_4, ADC_ADACK_4_0, ADC_ADACK_5_2 and ADC_ADACK_6_2,
     * where the numbers are the frequency of the ADC clock (ADCK) in MHz and are independent on the bus speed.
     * This is useful if you are using the Teensy at a very low clock frequency but want faster conversions,
     * but if F_BUS<F_ADCK, you can't use ADC_VERY_HIGH_SPEED for sampling speed.
     *
     */
    adc_->setConversionSpeed((ADC_CONVERSION_SPEED)speed, adc_num);
  }

  void setReference(uint8_t type, int8_t adc_num) {
    //! Set the voltage reference you prefer, default is 3.3 V (VCC)
    /*!
     * \param type can be ADC_REF_3V3, ADC_REF_1V2 (not for Teensy LC) or ADC_REF_EXT.
     *
     *  It recalibrates at the end.
     */
    adc_->setReference((ADC_REFERENCE)type, adc_num);
  }

  //! Sets the sampling speed
  /** Increase the sampling speed for low impedance sources, decrease it for higher impedance ones.
  * \param speed can be ADC_VERY_LOW_SPEED, ADC_LOW_SPEED, ADC_MED_SPEED, ADC_HIGH_SPEED or ADC_VERY_HIGH_SPEED.
  *
  * ADC_VERY_LOW_SPEED is the lowest possible sampling speed (+24 ADCK).
  * ADC_LOW_SPEED adds +16 ADCK.
  * ADC_MED_SPEED adds +10 ADCK.
  * ADC_HIGH_SPEED (or ADC_HIGH_SPEED_16BITS) adds +6 ADCK.
  * ADC_VERY_HIGH_SPEED is the highest possible sampling speed (0 ADCK added).
  */
  void setSamplingSpeed(uint8_t speed, int8_t adc_num) {
    adc_->setSamplingSpeed((ADC_SAMPLING_SPEED)speed, adc_num);
  }

  uint32_t benchmark_flops(uint32_t N) {
    float a = 1e6;
    float b = 1e7;
    uint32_t start = micros();
    for (uint32_t i = 0; i < N; i++) {
      a /= b;
    }
    return (micros() - start);
  }

  uint32_t benchmark_iops(uint32_t N) {
    uint32_t a = 1e6;
    uint32_t b = 1e7;
    uint32_t start = micros();
    for (uint32_t i = 0; i < N; i++) {
      a /= b;
    }
    return (micros() - start);
  }

  //! Set the number of averages
  /*!
  * \param num can be 0, 4, 8, 16 or 32.
  */
  void setAveraging(uint8_t num, int8_t adc_num) {
    adc_->setAveraging(num, adc_num);
  }


  //! Enable interrupts
  /** An IRQ_ADC0 Interrupt will be raised when the conversion is completed
  *  (including hardware averages and if the comparison (if any) is true).
  */
  void enableInterrupts(int8_t adc_num) {
    adc_->enableInterrupts(adc_num);
  }

  //! Disable interrupts
  void disableInterrupts(int8_t adc_num) {
    adc_->disableInterrupts(adc_num);
  }


  //! Enable DMA request
  /** An ADC DMA request will be raised when the conversion is completed
  *  (including hardware averages and if the comparison (if any) is true).
  */
  void enableDMA(int8_t adc_num) {
    adc_->enableDMA(adc_num);
  }

  //! Disable ADC DMA request
  void disableDMA(int8_t adc_num) {
    adc_->disableDMA(adc_num);
  }


  //! Enable the compare function to a single value
  /** A conversion will be completed only when the ADC value
  *  is >= compValue (greaterThan=1) or < compValue (greaterThan=0)
  *  Call it after changing the resolution
  *  Use with interrupts or poll conversion completion with isComplete()
  */
  void enableCompare(int16_t compValue, bool greaterThan, int8_t adc_num) {
    adc_->enableCompare(compValue, greaterThan, adc_num);
  }

  //! Enable the compare function to a range
  /** A conversion will be completed only when the ADC value is inside (insideRange=1) or outside (=0)
  *  the range given by (lowerLimit, upperLimit),including (inclusive=1) the limits or not (inclusive=0).
  *  See Table 31-78, p. 617 of the freescale manual.
  *  Call it after changing the resolution
  *  Use with interrupts or poll conversion completion with isComplete()
  */
  void enableCompareRange(int16_t lowerLimit, int16_t upperLimit, bool insideRange, bool inclusive, int8_t adc_num) {
    adc_->enableCompareRange(lowerLimit, upperLimit, insideRange, inclusive, adc_num);
  }

  //! Disable the compare function
  void disableCompare(int8_t adc_num) {
    adc_->disableCompare(adc_num);
  }


  //! Enable and set PGA
  /** Enables the PGA and sets the gain
  *   Use only for signals lower than 1.2 V
  *   \param gain can be 1, 2, 4, 8, 16, 32 or 64
  *
  */
  void enablePGA(uint8_t gain, int8_t adc_num) {
    adc_->enablePGA(gain, adc_num);
  }

  //! Returns the PGA level
  /** PGA level = from 1 to 64
  */
  uint8_t getPGA(int8_t adc_num) {
    return adc_->getPGA(adc_num);
  }

  //! Disable PGA
  void disablePGA(int8_t adc_num) {
    adc_->disablePGA(adc_num);
  }



  ////////////// INFORMATION ABOUT THE STATE OF THE ADC /////////////////

  //! Is the ADC converting at the moment?
  bool isConverting(int8_t adc_num) {
    return adc_->isConverting(adc_num);
  }

  //! Is an ADC conversion ready?
  /**
  *  \return 1 if yes, 0 if not.
  *  When a value is read this function returns 0 until a new value exists
  *  So it only makes sense to call it with continuous or non-blocking methods
  */
  bool isComplete(int8_t adc_num) {
    return adc_->isComplete(adc_num);
  }

  //! Is the ADC in differential mode?
  bool isDifferential(int8_t adc_num) {
    return adc_->isDifferential(adc_num);
  }

  //! Is the ADC in continuous mode?
  bool isContinuous(int8_t adc_num) {
    return adc_->isContinuous(adc_num);
  }



  //////////////// BLOCKING CONVERSION METHODS //////////////////

  //! Returns the analog value of the pin.
  /** It waits until the value is read and then returns the result.
  * If a comparison has been set up and fails, it will return ADC_ERROR_VALUE.
  * This function is interrupt safe, so it will restore the adc to the state it was before being called
  * If more than one ADC exists, it will select the module with less workload, you can force a selection using
  * adc_num. If you select ADC1 in Teensy 3.0 it will return ADC_ERROR_VALUE.
  */
  int analogRead(uint8_t pin, int8_t adc_num) {
    return adc_->analogRead(pin, adc_num);
  }

  //! Reads the differential analog value of two pins (pinP - pinN).
  /** It waits until the value is read and then returns the result.
  * If a comparison has been set up and fails, it will return ADC_ERROR_VALUE.
  * \param pinP must be A10 or A12.
  * \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
  * Other pins will return ADC_ERROR_VALUE.
  *
  * This function is interrupt safe, so it will restore the adc to the state it was before being called
  * If more than one ADC exists, it will select the module with less workload, you can force a selection using
  * adc_num
  */
  int analogReadDifferential(uint8_t pinP, uint8_t pinN, int8_t adc_num) {
    return adc_->analogReadDifferential(pinP, pinN, adc_num);
  }


  /////////////// NON-BLOCKING CONVERSION METHODS //////////////

  //! Starts an analog measurement on the pin and enables interrupts.
  /** It returns inmediately, get value with readSingle().
  *   If the pin is incorrect it returns ADC_ERROR_VALUE
  *   If this function interrupts a measurement, it stores the settings in adc_config
  */
  bool startSingleRead(uint8_t pin, int8_t adc_num) {
    return adc_->startSingleRead(pin, adc_num);
  }

  //! Start a differential conversion between two pins (pinP - pinN) and enables interrupts.
  /** It returns inmediately, get value with readSingle().
  *   \param pinP must be A10 or A12.
  *   \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
  *
  *   Other pins will return ADC_ERROR_DIFF_VALUE.
  *   If this function interrupts a measurement, it stores the settings in adc_config
  */
  bool startSingleDifferential(uint8_t pinP, uint8_t pinN, int8_t adc_num) {
    return adc_->startSingleDifferential(pinP, pinN, adc_num);
  }

  //! Reads the analog value of a single conversion.
  /** Set the conversion with with startSingleRead(pin) or startSingleDifferential(pinP, pinN).
  *   \return the converted value.
  */
  int readSingle(int8_t adc_num) {
    return adc_->readSingle(adc_num);
  }



  ///////////// CONTINUOUS CONVERSION METHODS ////////////

  //! Starts continuous conversion on the pin.
  /** It returns as soon as the ADC is set, use analogReadContinuous() to read the value.
  */
  bool startContinuous(uint8_t pin, int8_t adc_num) {
    return adc_->startContinuous(pin, adc_num);
  }

  //! Starts continuous conversion between the pins (pinP-pinN).
  /** It returns as soon as the ADC is set, use analogReadContinuous() to read the value.
  * \param pinP must be A10 or A12.
  * \param pinN must be A11 (if pinP=A10) or A13 (if pinP=A12).
  * Other pins will return ADC_ERROR_DIFF_VALUE.
  */
  bool startContinuousDifferential(uint8_t pinP, uint8_t pinN, int8_t adc_num) {
    return adc_->startContinuousDifferential(pinP, pinN, adc_num);
  }

  //! Reads the analog value of a continuous conversion.
  /** Set the continuous conversion with with analogStartContinuous(pin) or startContinuousDifferential(pinP, pinN).
  *   \return the last converted value.
  *   If single-ended and 16 bits it's necessary to typecast it to an unsigned type (like uint16_t),
  *   otherwise values larger than 3.3/2 V are interpreted as negative!
  */
  int analogReadContinuous(int8_t adc_num) {
    return adc_->analogReadContinuous(adc_num);
  }

  //! Stops continuous conversion
  void stopContinuous(int8_t adc_num) {
    adc_->stopContinuous(adc_num);
  }



  /////////// SYNCHRONIZED METHODS ///////////////
  ///// IF THE BOARD HAS ONLY ONE ADC, THEY ARE EMPYT METHODS /////

  //! Struct for synchronous measurements
  /** result_adc0 has the result from ADC0 and result_adc1 from ADC1.
  */

  //////////////// SYNCHRONIZED BLOCKING METHODS //////////////////

  //! Returns the analog values of both pins, measured at the same time by the two ADC modules.
  /** It waits until the values are read and then returns the result as a struct Sync_result,
  *   use Sync_result.result_adc0 and Sync_result.result_adc1.
  * If a comparison has been set up and fails, it will return ADC_ERROR_VALUE in both fields of the struct.
  * This function is interrupt safe, so it will restore the adc to the state it was before being called
  */
  Int32Array analogSynchronizedRead(uint8_t pin0, uint8_t pin1) {
    UInt8Array buffer = get_buffer();
    ADC::Sync_result &result = *reinterpret_cast<ADC::Sync_result *>(&buffer.data[0]);
    Int32Array output;
    output.length = sizeof(ADC::Sync_result) / sizeof(int32_t);
    output.data = reinterpret_cast<int32_t *>(&result);

    result = adc_->analogSynchronizedRead(pin0, pin1);
    return output;
  }
#if 0

  Int32Array analogSyncRead(uint8_t pin0, uint8_t pin1) __attribute__((always_inline)) {
    return analogSynchronizedRead(pin0, pin1);
  }

  //! Returns the differential analog values of both sets of pins, measured at the same time by the two ADC modules.
  /** It waits until the values are read and then returns the result as a struct Sync_result,
  *   use Sync_result.result_adc0 and Sync_result.result_adc1.
  * If a comparison has been set up and fails, it will return ADC_ERROR_VALUE in both fields of the struct.
  * This function is interrupt safe, so it will restore the adc to the state it was before being called
  */
  Int32Array analogSynchronizedReadDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {
    UInt8Array buffer = get_buffer();
    Int32Array output;
    output.length = sizeof(Sync_result) / sizeof(int32_t);
    output.data = reinterpret_cast<int32_t *>(&result);
    Sync_result &result = *reinterpret_cast<Sync_result *>(&buffer.data[0]);

    result = adc_->analogSynchronizedReadDifferential(pin0P, pin0N, pin1P, pin1N);
    return result
  }

  Int32Array analogSyncReadDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) __attribute__((always_inline)) {
      return analogSynchronizedReadDifferential(pin0P, pin0N, pin1P, pin1N);
  }

  /////////////// SYNCHRONIZED NON-BLOCKING METHODS //////////////

  //! Starts an analog measurement at the same time on the two ADC modules
  /** It returns inmediately, get value with readSynchronizedSingle().
  *   If the pin is incorrect it returns false
  *   If this function interrupts a measurement, it stores the settings in adc_config
  */
  bool startSynchronizedSingleRead(uint8_t pin0, uint8_t pin1) {
    return adc_->startSynchronizedSingleRead(pin0, pin1);
  }

  //! Start a differential conversion between two pins (pin0P - pin0N) and (pin1P - pin1N)
  /** It returns inmediately, get value with readSynchronizedSingle().
  *   \param pin0P, pin1P must be A10 or A12.
  *   \param pin0N, pin1N must be A11 (if pinP=A10) or A13 (if pinP=A12).
  *   Other pins will return false.
  *   If this function interrupts a measurement, it stores the settings in adc_config
  */
  bool startSynchronizedSingleDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {
    return adc_->startSynchronizedSingleDifferential(pin0P, pin0N, pin1P, pin1N);
  }

  //! Reads the analog value of a single conversion.
  /**
  *   \return the converted value.
  */
  Int32Array readSynchronizedSingle() {
    UInt8Array buffer = get_buffer();
    Int32Array output;
    output.length = sizeof(ADC::Sync_result) / sizeof(int32_t);
    output.data = reinterpret_cast<int32_t *>(&result);
    ADC::Sync_result &result = *reinterpret_cast<ADC::Sync_result *>(&buffer.data[0]);

    result = adc_->readSynchronizedSingle();
    return result;
  }


  ///////////// SYNCHRONIZED CONTINUOUS CONVERSION METHODS ////////////

  //! Starts a continuous conversion in both ADCs simultaneously
  /** Use readSynchronizedContinuous to get the values
  *
  */
  bool startSynchronizedContinuous(uint8_t pin0, uint8_t pin1) {
    return adc_->startSynchronizedContinuous(pin0, pin1);
  }

  //! Starts a continuous differential conversion in both ADCs simultaneously
  /** Use readSynchronizedContinuous to get the values
  *
  */
  bool startSynchronizedContinuousDifferential(uint8_t pin0P, uint8_t pin0N, uint8_t pin1P, uint8_t pin1N) {
    return adc_->startSynchronizedContinuousDifferential(pin0P, pin0N, pin1P, pin1N);
  }

  //! Returns the values of both ADCs.
  Int32Array readSynchronizedContinuous() {
    UInt8Array buffer = get_buffer();
    Int32Array output;
    output.length = sizeof(Sync_result) / sizeof(int32_t);
    output.data = reinterpret_cast<int32_t *>(&result);
    Sync_result &result = *reinterpret_cast<Sync_result *>(&buffer.data[0]);

    result = adc_->readSynchronizedContinuous();
    return result;
  }

  //! Stops synchronous continuous conversion
  void stopSynchronizedContinuous() {
    adc_->stopSynchronizedContinuous();
  }
#endif

  uint16_t analog_input_to_digital_pin(uint16_t pin) { return analogInputToDigitalPin(pin); }
  uint16_t digital_pin_has_pwm(uint16_t pin) { return digitalPinHasPWM(pin); }
  uint16_t digital_pin_to_interrupt(uint16_t pin) { return digitalPinToInterrupt(pin); }

  UInt8Array read_adc_registers(uint8_t adc_num) {
    return teensy::adc::serialize_registers(adc_num, get_buffer());
  }
  int8_t update_adc_registers(uint8_t adc_num, UInt8Array serialized_adc_msg) {
    return teensy::adc::update_registers(adc_num, serialized_adc_msg);
  }

  UInt8Array read_pit_registers() {
    return teensy::pit::serialize_registers(get_buffer());
  }
  int8_t update_pit_registers(UInt8Array serialized_pit_msg) {
    return teensy::pit::update_registers(serialized_pit_msg);
  }
  UInt8Array read_pit_timer_config(uint8_t timer_index) {
    return teensy::pit::serialize_timer_config(timer_index, get_buffer());
  }
  int8_t update_pit_timer_config(uint32_t index,
                                 UInt8Array serialized_config) {
    return teensy::pit::update_timer_config(index, serialized_config);
  }

  uint16_t dma_channel_count() { return DMA_NUM_CHANNELS; }
  UInt8Array read_dma_TCD(uint8_t channel_num) {
    return teensy::dma::serialize_TCD(channel_num, get_buffer());
  }
  void reset_dma_TCD(uint8_t channel_num) {
    teensy::dma::reset_TCD(channel_num);
  }
  int8_t update_dma_TCD(uint8_t channel_num, UInt8Array serialized_tcd) {
    return teensy::dma::update_TCD(channel_num, serialized_tcd);
  }
  UInt8Array read_dma_priority(uint8_t channel_num) {
    return teensy::dma::serialize_dchpri(channel_num, get_buffer());
  }
  UInt8Array read_dma_registers() {
    return teensy::dma::serialize_registers(get_buffer());
  }
  int8_t update_dma_registers(UInt8Array serialized_dma_msg) {
    return teensy::dma::update_registers(serialized_dma_msg);
  }
  UInt8Array read_dma_mux_chcfg(uint8_t channel_num) {
    return teensy::dma::serialize_mux_chcfg(channel_num, get_buffer());
  }
  int8_t update_dma_mux_chcfg(uint8_t channel_num, UInt8Array serialized_mux) {
    return teensy::dma::update_mux_chcfg(channel_num, serialized_mux);
  }
  void clear_dma_errors() {
    DMA_CERR = DMA_CERR_CAEI;  // Clear All Error Indicators
  }

  UInt8Array read_sim_SCGC6() { return teensy::sim::serialize_SCGC6(get_buffer()); }
  UInt8Array read_sim_SCGC7() { return teensy::sim::serialize_SCGC7(get_buffer()); }
  int8_t update_sim_SCGC6(UInt8Array serialized_scgc6) {
    return teensy::sim::update_SCGC6(serialized_scgc6);
  }
  int8_t update_sim_SCGC7(UInt8Array serialized_scgc7) {
    return teensy::sim::update_SCGC7(serialized_scgc7);
  }

  void free_all() {
    while (allocations_.size() > 0) { free((void *)allocations_.shift()); }
    while (aligned_allocations_.size() > 0) {
      aligned_free((void *)aligned_allocations_.shift());
    }
  }
  uint32_t mem_alloc(uint32_t size) {
    uint32_t address = (uint32_t)malloc(size);
    // Save to list of allocations for memory management.
    allocations_.add(address);
    return address;
  }
  void mem_free(uint32_t address) {
    for (int i = 0; i < allocations_.size(); i++) {
      if (allocations_.get(i) == address) { allocations_.remove(i); }
    }
    free((void *)address);
  }
  uint32_t mem_aligned_alloc(uint32_t alignment, uint32_t size) {
    uint32_t address = (uint32_t)aligned_malloc(alignment, size);
    // Save to list of allocations for memory management.
    aligned_allocations_.add(address);
    return address;
  }
  void mem_aligned_free(uint32_t address) {
    for (int i = 0; i < aligned_allocations_.size(); i++) {
      if (aligned_allocations_.get(i) == address) {
        aligned_allocations_.remove(i);
      }
    }
    aligned_free((void *)address);
  }
  uint32_t mem_aligned_alloc_and_set(uint32_t alignment, UInt8Array data) {
    // Allocate aligned memory.
    const uint32_t address = mem_aligned_alloc(alignment, data.length);
    if (!address) { return 0; }
    // Copy data to allocated memory.
    mem_cpy_host_to_device(address, data);
    return address;
  }
  void mem_cpy_host_to_device(uint32_t address, UInt8Array data) {
    memcpy((uint8_t *)address, data.data, data.length);
  }
  UInt8Array mem_cpy_device_to_host(uint32_t address, uint32_t size) {
    UInt8Array output;
    output.length = size;
    output.data = (uint8_t *)address;
    return output;
  }
  void mem_fill_uint8(uint32_t address, uint8_t value, uint32_t size) {
    mem_fill((uint8_t *)address, value, size);
  }
  void mem_fill_uint16(uint32_t address, uint16_t value, uint32_t size) {
    mem_fill((uint16_t *)address, value, size);
  }
  void mem_fill_uint32(uint32_t address, uint32_t value, uint32_t size) {
    mem_fill((uint32_t *)address, value, size);
  }
  void mem_fill_float(uint32_t address, float value, uint32_t size) {
    mem_fill((float *)address, value, size);
  }
  void loop() {
    if (dma_channel_done_ >= 0) {
      // DMA channel has completed.
      last_dma_channel_done_ = dma_channel_done_;
      dma_channel_done_ = -1;
    }
  }
  int8_t last_dma_channel_done() const { return last_dma_channel_done_; }
  void attach_dma_interrupt(uint8_t dma_channel) {
    void (*isr)(void);
    switch(dma_channel) {
      case 0: isr = &dma_ch0_isr; break;
      case 1: isr = &dma_ch1_isr; break;
      case 2: isr = &dma_ch2_isr; break;
      case 3: isr = &dma_ch3_isr; break;
      case 4: isr = &dma_ch4_isr; break;
      case 5: isr = &dma_ch5_isr; break;
      case 6: isr = &dma_ch6_isr; break;
      case 7: isr = &dma_ch7_isr; break;
      case 8: isr = &dma_ch8_isr; break;
      case 9: isr = &dma_ch9_isr; break;
      case 10: isr = &dma_ch10_isr; break;
      case 11: isr = &dma_ch11_isr; break;
      case 12: isr = &dma_ch12_isr; break;
      case 13: isr = &dma_ch13_isr; break;
      case 14: isr = &dma_ch14_isr; break;
      case 15: isr = &dma_ch15_isr; break;
      default: return;
    }
    _VectorsRam[dma_channel + IRQ_DMA_CH0 + 16] = isr;
    NVIC_ENABLE_IRQ(IRQ_DMA_CH0 + dma_channel);
  }

  void detach_dma_interrupt(uint8_t dma_channel) {
      NVIC_DISABLE_IRQ(IRQ_DMA_CH0 + dma_channel);
  }

};

}  // namespace dropbot_dx


#endif  // #ifndef ___NODE__H___
