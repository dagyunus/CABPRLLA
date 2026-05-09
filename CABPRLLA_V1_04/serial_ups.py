import logging
import random
import time
import serial 
import os
from crccheck.crc import Crc16, Crc16Buypass
######
import random
import logging
from threading import Thread
import struct
import requests
# import psycopg2
try:
    import RPi.GPIO as GPIO 
except ImportError:
    class DummyGPIO:
        BCM = 'BCM'; OUT = 'OUT'; HIGH = 1; LOW = 0
        def setmode(self, mode): pass
        def setup(self, pin, mode): pass
        def output(self, pin, val): pass
        def setwarnings(self, val): pass
    GPIO = DummyGPIO()
from datetime import datetime as dt
import json
# logging.basicConfig(filename='logs/serial.log', filemode='a', format='%(asctime)s - %(message)s', level=logging.DEBUG)
import traceback
import copy
#import logging
import socketio

# Logger'ları yapılandır
def setup_logger(name, log_file, level=logging.INFO):
    """Her kategori için farklı bir logger oluşturan yardımcı fonksiyon."""
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    handler = logging.FileHandler(log_file, mode='a')        
    handler.setFormatter(formatter)

    logger = logging.getLogger(name)
    logger.setLevel(level)
    logger.addHandler(handler)
    return logger

# 3 Farklı log dosyası için logger'lar
task_logger = setup_logger('task_log', 'task_log.log')
periodic_logger = setup_logger('periodic_log', 'get_periyodik_log.log')
events_logger = setup_logger('events_log', 'events_log.log')


rect_length = 60 + 30 #config
inv_length = 82 + 30 #67 + 30 #config
event_size = 16
is_dfu_active = False

events_control={
    "module_id":0,
    "rectifier": 0,
    "inverter": 0
}

read_module_config = []
check_module_data = []

rectifier_json = {
    "header": { # 5
        "id": 0.0,
        "status": 0.0,
        "fw_version": 0.0,
        "rtc": 0,
        "numberofeventstoread": 0.0
    },
   "rectifier": { #12
      "l1_freq": 0.0,
      "l2_freq": 0.0,
      "l3_freq": 0.0,
      "l1_irms": 0.0,
      "l2_irms": 0.0,
      "l3_irms": 0.0,
      "l1_vrms": 0.0,
      "l2_vrms": 0.0,
      "l3_vrms": 0.0,
      "l1_prms": 0.0,
      "l2_prms": 0.0,
      "l3_prms": 0.0
    },
     "batteries": { #6
      "pos_current": 0.0,
      "neg_current": 0.0,
      "neg_voltage": 0.0,
      "pos_voltage": 0.0,
      "backup_time": 0.0,
      "capacity": 0.0
    },
    "dc": { #2
      "neg_voltage": 0.0,
      "pos_voltage": 0.0
    },
    "flags": { # 23
        "recti_is_shutdown": False,
        "recti_is_fault": False,
        "recti_switching_en": False,
        "precharge_rlys_sc": False,
        "line_rlys_sc": False,
        "battery_rlys_sc": False,
        "dcdc_is_enabled": False,
        "dcdc_is_charge": False,
        "dcdc_is_discharge": False,
        "offset_defvals_inuse": False,
        "gains_defvals_inuse": False,
        "offset_calib_inprogress": False,
        "gain_calib_inprogress": False,
        "offset_calib_successfull": False,
        "gains_calib_successfull": False,
        "offset_calibs_memfault": False,
        "gain_calibs_memfault": False,
        "offset_calibs_limfault": False,
        "gain_calibs_limfault": False,
        "invalid_calib_request": False,
        "invalid_erase_request": False,
        "cmd_battery_test": False,
        "status_battery_test": False,
        "main_is_ok": True,
        "battery_is_ok": True,
    },
    "usercmd": { # 10
        "force_shutdown": False,
        "release_shutdown": False,
        "clear_fault": False,
        "offset_calib_trigger": False,
        "gain_calib_trigger": False,
        "erase_gain_calibrations": False,
        "erase_offs_calibrations": False,
        "erase_events": False,
        "dspl_blockchargedischarge": False,
        "dspl_unblockchargedischarge": False,
    },
    "userdata": { # 4
        "main_volts_median": 0,
        "main_volts_deviation": 0,
        "dcbara_refv": 0,
        "is_separete_batteries": False
    },
    "indiv_userdata": { # 4
        "bats_dischargeminvoltage": 0.0,
        "bats_floatchargevoltage": 0.0,
        "bats_ah": 0.0,
        "bats_numofconnectedbatteries": 0,
    },
     "calib_data": {
        "recti_irms_l1": 0.0,
        "recti_irms_l2": 0.0,
        "recti_irms_l3": 0.0,
        "recti_vrms_l1": 0.0,
        "recti_vrms_l2": 0.0,
        "recti_vrms_l3": 0.0,
        "recti_prms_l1": 0.0,
        "recti_prms_l2": 0.0,
        "recti_prms_l3": 0.0,
        "batt_pos_v": 0.0,
        "batt_pos_curr": 0.0,
        "batt_neg_v": 0.0,
        "batt_neg_curr": 0.0,
        "dccap_pos_v": 0.0,
        "dccap_neg_v": 0.0
    },
    "calib_command": {
        "offset_calib_trigger": False,
        "gain_calib_trigger": False,
        "erase_gain_calibrations": False,
        "erase_offs_calibrations": False
    },
    "calib_flag": {
        "offset_defvals_inuse": False,
        "gains_defvals_inuse": False,
        "offset_calib_inprogress": False,
        "gain_calib_inprogress": False,
        "offset_calib_successfull": False,
        "gains_calib_successfull": False,
        "offset_calibs_memfault": False,
        "gain_calibs_memfault": False,
        "offset_calibs_limfault": False,
        "gain_calibs_limfault": False,
        "invalid_calib_request": False,
        "invalid_erase_request": False
    }
}

inverter_json = {
    "header": { #4
        "id": 0.0,
        "status": 0.0,
        "fw_version": 0.0,
        "numberofeventstoread": 0.0
    },
    "bypass": {
      "l1_freq": 0.0,
      "l2_freq": 0.0,
      "l3_freq": 0.0,
      "l1_irms": 0.0,
      "l2_irms": 0.0,
      "l3_irms": 0.0,
      "l1_vrms": 0.0,
      "l2_vrms": 0.0,
      "l3_vrms": 0.0,
      "l1_prms": 0.0,
      "l2_prms": 0.0,
      "l3_prms": 0.0
    },
    "inverter": {
      "l1_freq": 0.0,
      "l2_freq": 0.0,
      "l3_freq": 0.0,
      "l1_irms": 0.0,
      "l2_irms": 0.0,
      "l3_irms": 0.0,
      "l1_vrms": 0.0,
      "l2_vrms": 0.0,
      "l3_vrms": 0.0,
      "l1_prms": 0.0,
      "l2_prms": 0.0,
      "l3_prms": 0.0,
    },
    "load": { #15
        "l1_pfactor": 0.0,
        "l2_pfactor": 0.0,
        "l3_pfactor": 0.0,
        "l1_loadrate": 0.0,
        "l2_loadrate": 0.0,
        "l3_loadrate": 0.0,
        "l1_preactive": 0.0,
        "l2_preactive": 0.0,
        "l3_preactive": 0.0,
        "l1_papperent": 0.0,
        "l2_papperent": 0.0,
        "l3_papperent": 0.0,
        "l1_pactive": 0.0,
        "l2_pactive": 0.0,
        "l3_pactive": 0.0
    },
    "temperatures": { # 5
        "ambient_temperature": 0.0,
        "recti_igbt_temperature": 0.0,
        "invrt_igbt_temperature": 0.0,
        "dcpos_igbt_temperature": 0.0,
        "dcneg_igbt_temperature": 0.0
    },
    "flags": { #23
        "inv_is_shutdown": True,
        "inv_is_fault": False,
        "inv_is_ecomode": True,
        "inv_is_master": False,
        "qout_active": False,
        "manbyp_is_active": False,
        "cpld_en": False,
        "thy_en": False,
        "bypass_rlys_sc": False,
        "load_rlys_sc": False,
        "offset_defvals_inuse": False,
        "gains_defvals_inuse": False,
        "offset_calib_inprogress": False,
        "gain_calib_inprogress": False,
        "offset_calib_succesfull": False,
        "gains_calib_succesfull": False,
        "offset_calibs_memfault": False,
        "gain_calibs_memfault": False,
        "offset_calibs_limfault": False,
        "gain_calibs_limfault": False,
        "invalid_calib_request": False,
        "invalid_erase_request": False,
        "cmd_battery_test": False,
        "bypass_is_ok": True,
        
    },
    "usercmd": { # 11
        "force_shutdown": False,
        "release_shutdown": False,
        "force_ecomode": False,
        "release_ecomode": False,
        "clear_fault": False,
        "offset_calib_trigger": False,
        "gain_calib_trigger": False,
        "erase_gain_calibrations": False,
        "erase_offs_calibrations": False,
        "erase_events": False
    },
    "userdata": { # 3
        "byps_volts_median": 0,
        "byps_volts_deviation": 0,
        "inv_outvoltage": 0
    },
    "calib_data": {
        "bypass_irms_l1": 0.0,
        "bypass_irms_l2": 0.0,
        "bypass_irms_l3": 0.0,
        "bypass_vrms_l1": 0.0,
        "bypass_vrms_l2": 0.0,
        "bypass_vrms_l3": 0.0,
        "bypass_prms_l1": 0.0,
        "bypass_prms_l2": 0.0,
        "bypass_prms_l3": 0.0,
        "inverter_irms_l1": 0.0,
        "inverter_irms_l2": 0.0,
        "inverter_irms_l3": 0.0,
        "inverter_vrms_l1": 0.0,
        "inverter_vrms_l2": 0.0,
        "inverter_vrms_l3": 0.0,
        "inverter_prms_l1": 0.0,
        "inverter_prms_l2": 0.0,
        "inverter_prms_l3": 0.0,
        "load_v_l1": 0.0,
        "load_v_l2": 0.0,
        "load_v_l3": 0.0,
        "inv_v_filt_l1": 0.0,
        "inv_v_filt_l2": 0.0,
        "inv_v_filt_l3": 0.0,
        "inv_mod_sin_l1": 0.0,
        "inv_mod_sin_l2": 0.0,
        "inv_mod_sin_l3": 0.0
    },
    "calib_command": {
        "offset_calib_trigger": False,
        "gain_calib_trigger": False,
        "erase_gain_calibrations": False,
        "erase_offs_calibrations": False
    },
    "calib_flag": {
        "offset_defvals_inuse": False,
        "gains_defvals_inuse": False,
        "offset_calib_inprogress": False,
        "gain_calib_inprogress": False,
        "offset_calib_successfull": False,
        "gains_calib_successfull": False,
        "offset_calibs_memfault": False,
        "gain_calibs_memfault": False,
        "offset_calibs_limfault": False,
        "gain_calibs_limfault": False,
        "invalid_calib_request": False,
        "invalid_erase_request": False
    }
}

# Socket bağlantısı kur
sio = socketio.Client()

selected_module = {
    "module_id": None,
    "time": {
        "hours": None,
        "minutes": None,
        "seconds": None
    },
    "interrupt": False
}

@sio.on('selected_module')
def on_selected_module(data):
    new_module_id = data['module_id']
    hours, minutes, seconds = map(int, data['timestamp'].split(":"))

    # Eğer saat, dakika veya saniye değiştiyse modül değişmiş demektir
    if (hours, minutes, seconds) != (selected_module["time"]["hours"], selected_module["time"]["minutes"], selected_module["time"]["seconds"]):
        print(f"Modül değişti: {new_module_id}")
        selected_module.update({
            "module_id": new_module_id,
            "time": {
                "hours": hours,
                "minutes": minutes,
                "seconds": seconds
            },
            "interrupt": True
        })

def find_event_log(event_id):
    try:
        # JSON dosyasını oku
        with open("events_log.json", 'r', encoding='utf-8') as file:
            data = json.load(file)
        
        # rec ve inv anahtarları altındaki arraylerde objeyi arama
        for key in ['rectifier', 'inverter']:
            if key in data and isinstance(data[key], list):
                for obj in data[key]:
                    if obj.get('eventId') == event_id:  # event_id eşleşmesini kontrol et
                        return obj
        
        return None  # Eğer eşleşen bir obje bulunamazsa None döndür
        
    except Exception as e:
        print(f"Hata: {e}")
        events_logger.error("find_event_log ERROR " + str(e))
        return None

def get_key_values(json_obj):
    key_values = []
    def traverse(obj, parent_key=""):
        for k, v in obj.items():
            full_key = f"{parent_key}.{k}" if parent_key else k
            if isinstance(v, dict):  # Eğer değer bir dict ise, recursive olarak çağır
                traverse(v, full_key)
            else:
                key_values.append((full_key, v))

    traverse(json_obj)
    return key_values


def update_json_values(json_obj, values):

    key_values = get_key_values(json_obj)
    keys = [key for key, _ in key_values]  # Sadece anahtarları al

    if len(values) != len(keys):
        raise ValueError("Array uzunluğu JSON içindeki değer sayısıyla eşleşmiyor!")

    # Anahtarları ve yeni değerleri eşleştirerek JSON'u güncelle
    updated_json = copy.deepcopy(json_obj)  # Orijinal JSON'u değiştirmemek için derin kopyala

    def update_keys(obj, keys_values):
        for k, v in obj.items():
            if isinstance(v, dict):  # Eğer değer bir dict ise, recursive olarak güncelle
                update_keys(v, keys_values)
            else:
                key, new_value = keys_values.pop(0)  # Sıradaki anahtar-değer çiftini al
                obj[k] = new_value  # Yeni değeri ata

    # Anahtarlarla array değerlerini eşleştir
    keys_values = list(zip(keys, values))
    update_keys(updated_json, keys_values)
    return updated_json




def handle_rectifier_data(raw):
    my_buffer = []
    next_index = 0

    # region rectifier parsing

    # region header, analyses, batteries, dc : map 4 bytes to float

    for i in range(25):
        tmp = convert_float(raw[next_index], raw[next_index + 1], raw[next_index + 2], raw[next_index + 3])
        my_buffer.append(tmp)
        next_index = next_index + 4
        

    # number of events
    no_of_events = int(float(my_buffer[4]))
    if no_of_events != 0:
        if no_of_events > 26:
            no_of_events = 26
        tmp = [int(float(my_buffer[0])), int(no_of_events)]  # id, no of events
        #events_control.append(tmp)

    # endregion

    # region flags : map 64 bits to booleans

    tmp = raw[next_index] + raw[next_index + 1] * 0x100  + raw[next_index + 2] * 0x10000 + raw[next_index + 3] * 0x1000000 # 4 bytes 
    next_index = next_index + 8  # due to 8 bytes of reserved block for the flag

    for i in range(25):
        if tmp & (0b1 << i) != 0:
            my_buffer.append(True)
        else:
            my_buffer.append(False)


    # endregion

    # region  user commands : map 32 bits to booleans

    tmp = raw[next_index]  # 1 lsB is taken into account because only 9 bits are used
    next_index = next_index + 4

    for i in range(10):
        if tmp & (0b1 << i) != 0:
            my_buffer.append(True)
        else:
            my_buffer.append(False)

    # endregion

    # region  user data common : map 2 bytes to positive integer

    user_data_start_index = next_index

    for i in range(3):
        tmp = raw[next_index] + raw[next_index + 1] * 0x100
        next_index = next_index + 2
        my_buffer.append(tmp)

    tmp = (raw[next_index] + raw[next_index + 1] * 0x100)
    if tmp & 0b1 != 0:
        my_buffer.append(False)
    else:
        my_buffer.append(True)

    next_index = user_data_start_index + 60

    # endregion

    # region  user data indiv : map 4 bytes to float

    for i in range(3):
        tmp = convert_float(raw[next_index], raw[next_index + 1], raw[next_index + 2], raw[next_index + 3])
        next_index = next_index + 4
        my_buffer.append(tmp)

    # endregion

    # region  user data indiv : map 2 bytes to positive integer

    tmp = raw[next_index] + raw[next_index + 1] * 0x100
    my_buffer.append(tmp)
    
    # next_index must be moved to the start of calibration data (User_Data is 120 bytes)
    next_index = user_data_start_index + 120

    # region calibration data : map 4 bytes to float
    for i in range(15):  # 15 float değer (kalibrasyon verileri)
        tmp = convert_float(raw[next_index], raw[next_index + 1], raw[next_index + 2], raw[next_index + 3])
        my_buffer.append(tmp)
        next_index = next_index + 4

    # region calibration commands : map 16 bits to booleans
    tmp = raw[next_index] + raw[next_index + 1] * 0x100
    next_index = next_index + 2

    for i in range(4):  # 4 komut
        if tmp & (0b1 << i) != 0:
            my_buffer.append(True)
        else:
            my_buffer.append(False)

    # region calibration flags : map 16 bits to booleans
    tmp = raw[next_index] + raw[next_index + 1] * 0x100
    next_index = next_index + 2

    for i in range(12):  # 12 bayrak
        if tmp & (0b1 << i) != 0:
            my_buffer.append(True)
        else:
            my_buffer.append(False)

    return update_json_values(rectifier_json,my_buffer)


def handle_inverter_data(raw):
    my_buffer = []
    next_index = 0
    # region inverter parsing

    # region header, analyses, load, temperature : map 4 bytes to float
    try:
        for i in range(48):
            tmp = convert_float(raw[next_index], raw[next_index + 1], raw[next_index + 2], raw[next_index + 3])
            my_buffer.append(tmp)
            next_index = next_index + 4

        # number of events
        no_of_events = int(float(my_buffer[3]))
        if no_of_events != 0:
            if no_of_events > 23:
                no_of_events = 23
            tmp = [int(float(my_buffer[0])), int(no_of_events)]  # id, no of events
            #events_control.append(tmp)

        # endregion

        # region flags : map 64 bits to booleans

        tmp = raw[next_index] + raw[next_index + 1] * 0x100 + raw[next_index + 2] * 0x10000 + raw[next_index + 3] * 0x1000000 # 4 bytes 
        next_index = next_index + 8  # due to 8 bytes of reserved block for the flag

        for i in range(24):
            if tmp & (0b1 << i) != 0:
                my_buffer.append(True)
            else:
                my_buffer.append(False)

        # endregion

        # region  user commands : map 32 bits to booleans

        tmp = raw[next_index]  # 1 lsB is taken into account because only 9 bits are used
        next_index = next_index + 4

        for i in range(10):
            if tmp & (0b1 << i) != 0:
                my_buffer.append(True)
            else:
                my_buffer.append(False)

        # endregion

        user_data_start_index = next_index
        for i in range(3):
            tmp = raw[next_index] + raw[next_index + 1] * 0x100
            next_index = next_index + 2
            my_buffer.append(tmp)
        
        # next_index must be moved to the start of calibration data (User_Data is 120 bytes)
        next_index = user_data_start_index + 120
    
        # region calibration data : map 4 bytes to float
        for i in range(27):  # 27 float değer (kalibrasyon verileri)
            tmp = convert_float(raw[next_index], raw[next_index + 1], raw[next_index + 2], raw[next_index + 3])
            my_buffer.append(tmp)
            next_index = next_index + 4

        # region calibration commands : map 16 bits to booleans
        tmp = raw[next_index] + raw[next_index + 1] * 0x100
        next_index = next_index + 2

        for i in range(4):  # 4 komut
            if tmp & (0b1 << i) != 0:
                my_buffer.append(True)
            else:
                my_buffer.append(False)

        # region calibration flags : map 16 bits to booleans
        tmp = raw[next_index] + raw[next_index + 1] * 0x100
        next_index = next_index + 2

        for i in range(12):  # 12 bayrak
            if tmp & (0b1 << i) != 0:
                my_buffer.append(True)
            else:
                my_buffer.append(False)

        #return my_buffer
        return update_json_values(inverter_json,my_buffer)
        # endregion

        # endregion
    except Exception as e:
        print(e) 
        periodic_logger.error("handle_inverter_data ERROR " + str(e))

def insert_periodic(data , status, shouldSend):
    """
    data = {
        "module_id": 1, 
        "rectifier": [[0, 0, 128, 63, 0, 0, 128, 63, 0, 0, 12, 66, 49, 75, 104, 98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 86, 35, 215, 60, 124, 65, 146, 61, 0, 192, 186, 62, 0, 192, 186, 190, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 230, 0, 15, 0, 94, 1, 0, 0, 0, 0, 178, 239, 50, 0, 0, 0, 136, 134, 59, 88, 204, 119, 70, 190, 231, 98, 57, 74, 172, 103, 115, 97, 162, 246, 136, 133, 36, 54, 177, 118, 148, 163, 164, 84, 228, 2, 180, 44, 145, 57, 73, 84, 137, 109, 5, 76, 83, 92, 9, 203, 0, 0, 48, 65, 0, 0, 88, 65, 0, 0, 96, 65, 50, 0, 0, 0, 64, 90, 192, 138, 43, 40, 123, 195, 235, 221, 111, 255, 141, 14, 109, 185, 161, 142, 206, 31, 72, 183, 188, 131, 33, 124, 158, 52, 65, 219, 175, 121, 24, 182, 234, 33, 27, 248, 158, 39, 75, 44, 157, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 84, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]], 
        "inverter": [[0, 0, 202, 66, 0, 0, 0, 65, 0, 0, 128, 63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 74, 196, 13, 143, 140, 191, 111, 71, 1, 110, 221, 96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 147, 71, 125, 227, 177, 20, 72, 167, 168, 141, 223, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 224, 61, 228, 65, 0, 21, 161, 65, 0, 21, 161, 65, 0, 157, 242, 65, 224, 116, 244, 65, 3, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 230, 0, 15, 0, 230, 0, 0, 0, 0, 0, 138, 230, 0, 0, 0, 0, 7, 28, 126, 7, 151, 244, 184, 45, 46, 250, 102, 224, 142, 79, 117, 30, 107, 118, 107, 145, 111, 127, 52, 179, 5, 223, 27, 29, 45, 188, 11, 119, 82, 111, 151, 227, 255, 233, 85, 120, 203, 9, 153, 187, 71, 98, 17, 94, 152, 202, 78, 220, 203, 134, 44, 121, 201, 164, 71, 86, 157, 245, 174, 15, 13, 254, 127, 235, 163, 100, 26, 2, 190, 97, 87, 173, 227, 12, 205, 160, 226, 236, 36, 127, 242, 199, 35, 255, 139, 196, 110, 180, 131, 74, 95, 192, 209, 144, 166, 194, 236, 199, 32, 203, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 255, 127, 255, 127, 255, 127, 0, 128, 0, 0, 255, 127, 0, 128, 0, 0, 255, 127, 0, 128, 0, 0, 0, 0, 0, 128, 0, 128, 255, 127, 255, 127, 0, 0, 0, 128, 0, 128, 160, 251, 255, 127, 1, 0, 0, 0, 0, 0, 0, 0]]
    },
    """
    try:
        if status == "active":
            isConnected = True
            rectifier_data = handle_rectifier_data(data["rectifier"][0])
            inverter_data = handle_inverter_data(data["inverter"][0])
        else :
            isConnected = False
            # Orijinal template'leri bozmamak için derin kopya kullanıyoruz
            rectifier_data = copy.deepcopy(rectifier_json)
            inverter_data = copy.deepcopy(inverter_json)
        
        timestamp_ms = int(time.time() * 1000)
        seconds = rectifier_data["header"]["rtc"] / 1e12  # picosaniyeden saniyeye
        rectifier_data["header"]["rtc"] = seconds

        module_data = {
            "module_id": data["module_id"],
            "rectifier": {
                "header" : rectifier_data["header"],
                "rectifier" : rectifier_data["rectifier"],
                "batteries" : rectifier_data["batteries"],
                "dc" : rectifier_data["dc"],
                "flags" : rectifier_data["flags"],
                "usercmd" : rectifier_data["usercmd"],
                "userdata" : rectifier_data["userdata"],
                "indiv_userdata" : rectifier_data["indiv_userdata"],
                "calib_data" : rectifier_data["calib_data"],
                "calib_command" : rectifier_data["calib_command"],
                "calib_flag" : rectifier_data["calib_flag"]
            },
            "inverter": {
                "header" : inverter_data["header"],
                "bypass" : inverter_data["bypass"],
                "inverter" : inverter_data["inverter"],
                "load" : inverter_data["load"],
                "temperatures" : inverter_data["temperatures"],
                "flags" : inverter_data["flags"],
                "usercmd" : inverter_data["usercmd"],
                "userdata" : inverter_data["userdata"],
                "calib_data" : inverter_data["calib_data"],
                "calib_command" : inverter_data["calib_command"],
                "calib_flag" : inverter_data["calib_flag"]
            },
            "applyFlags": {
                "userCmd": False,
                "userData": False,
                "indivUserData": False,
                "calibData": False,
                "calibCommand": False,
                "rtc": False,
                "addressing": False,
                "allModules": False
            },
            "isConnected":isConnected,
            "timestamp": timestamp_ms,
            "installationDate": timestamp_ms
        }
        check_module_data.append(module_data)
        
        if shouldSend:
            module_conf = {
                "module_id": data["module_id"],
                "rectifier": {
                    "header" : rectifier_data["header"],
                    "usercmd" : rectifier_data["usercmd"],
                    "userdata" : rectifier_data["userdata"],
                    "indiv_userdata" : rectifier_data["indiv_userdata"],
                    "flags" : rectifier_data["flags"]
                },
                "inverter": {
                    "header" : inverter_data["header"],
                    "usercmd" : inverter_data["usercmd"],
                    "userdata" : inverter_data["userdata"],
                    "flags" : inverter_data["flags"]
                },
                "applyFlags": {
                    "userCmd": False,
                    "userData": False,
                    "indivUserData": False,
                    "calibData": False,
                    "calibCommand": False,
                    "rtc": False,
                    "addressing": False,
                    "allModules": False,
                },
                "isSend":False,
                "timestamp": timestamp_ms,
                "installationDate":timestamp_ms
            }
            
            if data["module_id"] == 0:
                module_conf.pop("module_id")
                module_conf["ups_id"] = data["module_id"] + 1
                module_conf["scanAllDevices"] = False
                write_data(module_conf, "ups-config")
            else:
                write_data(module_conf, "module-config")
        if int(rectifier_data['header']['numberofeventstoread']) > 0 or int(inverter_data['header']['numberofeventstoread']) > 0:
           
            events_control = {
                "module_id": data["module_id"],
                "rectifier": get_events( int(rectifier_data['header']['id']), int(rectifier_data['header']['numberofeventstoread'])),
                "inverter": get_events(int(inverter_data['header']['id']), int(inverter_data['header']['numberofeventstoread'])) ,    
                "isAlarm": True,
                "timestamp": timestamp_ms
            }

            write_data(events_control, "event-log")
            
        if data["module_id"] == 0:
            module_data.pop("module_id")
            module_data["ups_id"] = data["module_id"] 
            write_data(module_data, "ups-data")
        else:
            write_data(module_data, "module-data") 

    except Exception as e:
        periodic_logger.error("insert_periodic ERROR " + str(e))


def convert_float(data1, data2, data3, data4):
    data = [data1, data2, data3, data4]
    # Bytearray oluştur ve float olarak çöz
    return_data = struct.unpack('<f', bytearray(data))[0]
    # 3 ondalık basamağa yuvarla
    return_data = round(return_data, 3)
    # Sıfır kontrolü
    return return_data if return_data != 0.0 else 0


def parse_and_insert_events(module_no, event):

    event_data= []
    for ev in event:
        try:
            my_buffer = []
            my_index = 0

            if module_no > 100:
                moduleType = "inverter"
            else:
                moduleType = "rectifier"

            my_buffer.append(module_no)

            my_buffer.append(str(ev[my_index] + ev[my_index + 1] * 0x100 + ev[my_index + 2] * 0x10000 +
                             + ev[my_index + 3] * 0x1000000))  # 4 LSB (module_rtc)

            my_index = my_index + 4

            my_buffer.append(ev[my_index] + (ev[my_index + 1] & 0b1111) * 0x100)  # next 12 LSb (event_id)
            my_buffer.append((ev[my_index + 1] >> 4) & 0b1111) # next 4 LSb (group)

            my_index = my_index + 2

            my_buffer.append(ev[my_index] & 0b111)  # next 3 LSb (type)
            my_buffer.append((ev[my_index] >> 3) & 0b111)  # next 3 LSb (action)

            if moduleType == "rectifier":
                # region rectifier events parsing

                # next 2 LSb (pre_ch_rl_sc, lx_rl_sc)
                for k in range(2):
                    if ev[my_index] & (0b1 << (k + 6)) != 0:
                        my_buffer.append(True)
                    else:
                        my_buffer.append(False)

                my_index = my_index + 1

                # next 2 LSb (batt_rl_sc, sw_en)
                for k in range(2):
                    if ev[my_index] & (0b1 << k) != 0:
                        my_buffer.append(True)
                    else:
                        my_buffer.append(False)

                temp = (ev[my_index] >> 2) & 0b11  # next 2 LSb (dcdc_state)
                if temp == 0:
                    my_buffer.append("NOOP")
                elif temp == 1:
                    my_buffer.append("CHARGE")
                elif temp == 2:
                    my_buffer.append("DISCHARGE")
                else:
                    my_buffer.append("ERROR")

                temp = (ev[my_index] >> 4) & 0b111  # next 3 LSb (state)
                if temp == 0:
                    my_buffer.append("STARTUP")
                elif temp == 1:
                    my_buffer.append("INIT")
                elif temp == 2:
                    my_buffer.append("ACTIVE")
                elif temp == 3:
                    my_buffer.append("DISCHARGE")
                elif temp == 4:
                    my_buffer.append("FAULT")
                else:
                    my_buffer.append("ERROR")

                my_index = my_index + 1

                for k in range(3):
                    my_buffer.append(str(ev[my_index] + ev[my_index + 1] * 0x100))  # next 2 LSB (data k)
                    my_index = my_index + 2

                temp = ""
                for my_byte in ev:
                    temp = temp + format(my_byte, 'x').zfill(2)
                my_buffer.append(temp)

                # endregion
                result_event = find_event_log(int(my_buffer[2]))
                if result_event:
                    rectifier_event = {
                    "module_id": my_buffer[0],
                    "module_rtc": my_buffer[1],
                    "event_id": my_buffer[2],
                    "group_no": my_buffer[3],
                    "type": my_buffer[4],
                    "action": my_buffer[5],
                    "pre_ch_rl_sc": my_buffer[6],
                    "lx_rl_sc": my_buffer[7],
                    "batt_rl_sc": my_buffer[8],
                    "sw_en": my_buffer[9],
                    "dcdc_state": my_buffer[10],
                    "state": my_buffer[11],
                    "data0": my_buffer[12],
                    "data1": my_buffer[13],
                    "data2": my_buffer[14],
                    "raw_data": my_buffer[15]
                    }
                    if isinstance(result_event, dict):
                        rectifier_event.update(result_event)
                    # endregion
                    event_data.append(rectifier_event)

            elif moduleType == "inverter":
                # region inverter events parsing

                # next 2 LSb (byp_rl_sc, load_rl_sc)
                for k in range(2):
                    if ev[my_index] & (0b1 << (k + 6)) != 0:
                        my_buffer.append(True)
                    else:
                        my_buffer.append(False)

                my_index = my_index + 1

                # next 2 LSb (cpld_en, thy_en)
                for k in range(2):
                    if ev[my_index] & (0b1 << k) != 0:
                        my_buffer.append(True)
                    else:
                        my_buffer.append(False)

                temp = (ev[my_index] >> 2) & 0b111  # next 3 LSb (state)
                if temp == 0:
                    my_buffer.append("STARTUP")
                elif temp == 1:
                    my_buffer.append("INIT")
                elif temp == 2:
                    my_buffer.append("ACTIVE")
                elif temp == 3:
                    my_buffer.append("DISCHARGE")
                elif temp == 4:
                    my_buffer.append("FAULT")
                else:
                    my_buffer.append("ERROR")

                # man_byp
                if (ev[my_index] >> 5) & 0b1 != 0:
                    my_buffer.append(True)
                else:
                    my_buffer.append(False)

                my_index = my_index + 1

                for k in range(3):
                    my_buffer.append(str(ev[my_index] + ev[my_index + 1] * 0x100))  # next 2 LSB (data k)
                    my_index = my_index + 2

                temp = ""
                for my_byte in ev:
                    temp = temp + format(my_byte, 'x').zfill(2)
                my_buffer.append(temp)
                
                # endregion
                result_event = find_event_log(int(my_buffer[2]))
                if result_event:
                    inverter_event = {
                    "module_id": my_buffer[0],
                    "module_rtc": my_buffer[1],
                    "event_id": my_buffer[2],
                    "group_no": my_buffer[3],
                    "type": my_buffer[4],
                    "action": my_buffer[5],
                    "byp_rl_sc": my_buffer[6],
                    "load_rl_sc": my_buffer[7],
                    "cpld_en": my_buffer[8],
                    "thy_en": my_buffer[9],
                    "state": my_buffer[10],
                    "man_byp": my_buffer[11],
                    "data0": my_buffer[12],
                    "data1": my_buffer[13],
                    "data2": my_buffer[14],
                    "raw_data": my_buffer[15]
                    }
                    if isinstance(result_event, dict):
                        inverter_event.update(result_event)
                    # endregion
                    event_data.append(inverter_event)
                    # endregion
            else:
                pass

        except Exception as e:
            events_logger.error("parse_and_insert_events ERROR " + str(e))
    return event_data

def get_events(module_no, number_of_events):
    events = []
    number_of_events = int(number_of_events)
    if number_of_events == 0:
        return []
    try:
        last_data = [int(module_no), 0x03, 0x00, 0x78, 0x00, int(number_of_events * event_size / 4)]  # * 4 for float
        crc = Crc16Buypass.calc(last_data)
        crc1 = (crc >> 8) & 0x00FF
        crc2 = crc & 0x00FF
        last_data.append(crc1)
        last_data.append(crc2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        ser.write(serial.to_bytes(last_data))
        time.sleep(0.01)
        GPIO.output(12, GPIO.LOW)
        response_data = ser.read(number_of_events * event_size + 6)
        # time.sleep(0.01)
        GPIO.output(12, GPIO.HIGH)
        # time.sleep(0.01)
        GPIO.output(12, GPIO.HIGH)
        for i in range(number_of_events):
            start = 4 + i * event_size
            events.append(response_data[start:start + 16])
    except Exception as e:
        GPIO.output(12, GPIO.HIGH)
        events_logger.error("(get_events) module_no = " + str(module_no) + " len(response_data) = " + str(len(response_data))
                     + " Exception: " + str(e.args))
    return parse_and_insert_events(module_no, events)


def convert_commands_to_bits(user_commands):
    # Bayrak değerlerini saklayan bir bit değişkeni
    bit_value = 0
    # Pozisyonları sırayla hesaplayarak işleme
    for position, flag in enumerate(user_commands.keys()):
        if user_commands.get(flag, False):  # Eğer bayrak True ise
            bit_value |= (1 << position)  # İlgili biti 1 yapar
        else:
            bit_value &= ~(1 << position)  # İlgili biti 0 yapar
    # Güncellenen bit_value'yu döndür
    return bit_value

def rectifier_user_cmd_bytestream(module_id, usercmd_dict):
    # Modbus 16, Başlangıç Adresi 50, 2 Register (4 Byte)
    # Register sayısı 2, Byte sayısı 4 olmalı
    retVal = [module_id, 16, 0, 27, 0, 4] 

    # Komutları bit tam sayısına çevir (Inverter sırasıyla)
    bits = convert_commands_to_bits(usercmd_dict) 

    # uint32_t (I) paketleme
    # Senin float fonksiyonundaki gibi [3, 2, 1, 0] sırasıyla diziyoruz
    byte_data = struct.pack('<I', bits)
    retVal.extend([byte_data[3], byte_data[2], byte_data[1], byte_data[0]])

    return retVal

def inverter_user_cmd_bytestream(module_id, usercmd_dict):
    # Modbus 16, Başlangıç Adresi 50, 2 Register (4 Byte)
    # Register sayısı 2, Byte sayısı 4 olmalı
    retVal = [module_id, 16, 0, 50, 0, 4] 

    # Komutları bit tam sayısına çevir (Inverter sırasıyla)
    bits = convert_commands_to_bits(usercmd_dict) 

    # uint32_t (I) paketleme
    # Senin float fonksiyonundaki gibi [3, 2, 1, 0] sırasıyla diziyoruz
    byte_data = struct.pack('<I', bits)
    retVal.extend([byte_data[3], byte_data[2], byte_data[1], byte_data[0]])

    return retVal
def send_user_commands(module):
    try:
        moduleType = ["rectifier", "inverter"]
        x = 1

        for m in moduleType:
            if m == "inverter":
                module_id = module["module_id"] + 100
                last_data = inverter_user_cmd_bytestream(module_id, module["inverter"]["usercmd"])
            else:
                module_id = module["module_id"]
                last_data = rectifier_user_cmd_bytestream(module_id, module["rectifier"]["usercmd"])
            
            try :
                #for i in range(0, len(last_data)):
                #    last_data[i] = int(last_data[i])
                crc = Crc16Buypass.calc(last_data)
                crc1 = (crc >> 8) & 0x00FF
                crc2 = crc & 0x00FF
                last_data.append(crc1)
                last_data.append(crc2)
                
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                ser.write(last_data)
                time.sleep(0.01)
                time.sleep(0.3)
                GPIO.output(12, GPIO.HIGH)
                
            except Exception as e:
                task_logger.error("(send_user_commands) Exception: " + str(e.args))
                GPIO.output(12, GPIO.HIGH)
    except Exception as e:
        task_logger.error("(send_user_commands) Exception: " + str(e.args))
        GPIO.output(12, GPIO.HIGH)
    """        
    if x == 1:
        new_module = copy.deepcopy(module)  # module'ün derin bir kopyasını oluştur
        new_module["module_id"] = 32
        new_module["timestamp"] = int(time.time() * 1000) - 10 * 60 * 1000
        read_module_config.insert(0, new_module)  # Kopyayı listeye ekle
        x = 10
    """       



def get_periodic_data():
    global is_dfu_active
    if is_dfu_active:
        return
    try:
        moduleType = ["rectifier", "inverter"]
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        for module in read_module_config:
            data = {"module_id": 0, "rectifier" : [] , "inverter" : [] }
            isActive = True
            if selected_module["interrupt"]:
                print("Read module kesiliyor...")
                selected_module["interrupt"] = False
                break

            for m in moduleType:
                raw_data = []
                #response_data = None
            
                if m == "rectifier":
                    module_id = module["module_id"]
                    my_length = rect_length * 4  # * 4 because of float
                else:
                    module_id = module["module_id"] + 100
                    my_length = inv_length * 4   # * 4 because of float
                
                last_data = [int(module_id), 0x03, 0x00, 0x00, 0x00, int(my_length / 4)]

                crc = Crc16Buypass.calc(last_data)
                crc1 = (crc >> 8) & 0x00FF
                crc2 = crc & 0x00FF
                last_data.append(crc1)
                last_data.append(crc2)
                
                ser.write(serial.to_bytes(last_data))
                time.sleep(0.03)

                GPIO.output(12, GPIO.LOW)
                response_data = ser.read(my_length + 6) 
                
                GPIO.output(12, GPIO.HIGH)
                
                #timestamp2 = int(time.time() * 1000)
                if len(response_data) != my_length + 6:
                    #if timestamp2 - module["timestamp"] >= 5 * 60 * 1000 :
                    time.sleep(0.05)
                    insert_periodic({"module_id":module["module_id"]}, "inActive", True)
                    isActive = False
                    break

            
                # crc calculation for response of the module
                crc_read = [response_data[my_length + 4], response_data[my_length + 5]]
                for k in range(my_length + 4):
                    raw_data.append(response_data[k])
                crc_calc = Crc16Buypass.calc(raw_data)
                crc_calc = [(crc_calc >> 8) & 0x00FF, crc_calc & 0x00FF]
                if crc_read != crc_calc:
                    time.sleep(1.00)
                    continue

                # get rid of anything besides data
                for k in range(4):
                    raw_data.pop(0)
                #insert_periodic(m, i, raw_data)
                data[m].append(raw_data)
                
                
            if isActive and len(data["rectifier"]) and len(data["inverter"]):       
                data["module_id"] = module["module_id"]  
                insert_periodic(data, "active", module["isSend"])
    except Exception as e:
        periodic_logger.error("(get_periodic_data) Exception:  " + str(e))
        GPIO.output(12, GPIO.HIGH)


def rectifier_common_bytestream(module_id, main_volts_median, main_volts_deviation, is_separete_batteries, dcbara_refv):

    retVal = [module_id, 16, 0, 28, 0, 4]

    def to_big_endian(value):
        """UInt16 değeri big-endian formatında 2 byte olarak döndürür (ModBus standardı)."""
        byte_data = struct.pack('<H', value)
        return [byte_data[1], byte_data[0]] 

    retVal.extend(to_big_endian(main_volts_deviation))
    retVal.extend(to_big_endian(main_volts_median))
    # 0 -> Shared/Common, 1 -> Separate (based on ModBus_R.h line 124)
    common_batteries_value = 0 if is_separete_batteries else 1 
    retVal.extend(to_big_endian(common_batteries_value))
    retVal.extend(to_big_endian(dcbara_refv))   
    

    return retVal


def inverter_common_bytestream(module_id, byps_volts_dev, byps_volts_median, inv_outvoltage):

    retVal = [module_id, 16, 0, 51, 0, 4]

    def to_little_endian(value):
        """UInt16 değeri little-endian formatında 2 byte olarak döndürür."""
        byte_data = struct.pack('<H', value)
        return [byte_data[1], byte_data[0]] 
    
    retVal.extend(to_little_endian(byps_volts_dev))
    
    retVal.extend(to_little_endian(byps_volts_median))
    
    retVal.extend([0, 0]) # 0 0 ekleniyor

    retVal.extend(to_little_endian(inv_outvoltage)) 

    return retVal


def send_common_user_data(module):
    try:
        moduleType = ["rectifier", "inverter"]

        for m in moduleType:
            if m == "inverter":
                module_id = module["module_id"] + 100
                byps_volts_deviation = module["inverter"]["userdata"]["byps_volts_deviation"] 
                byps_volts_median = module["inverter"]["userdata"]["byps_volts_median"]
                inv_outvoltage = module["inverter"]["userdata"]["inv_outvoltage"]

                byte_stream = inverter_common_bytestream(module_id, byps_volts_deviation, 
                    byps_volts_median, inv_outvoltage)
            else:
                module_id = module["module_id"]
                main_volts_deviation = module["rectifier"]["userdata"]["main_volts_deviation"]
                main_volts_median = module["rectifier"]["userdata"]["main_volts_median"]
                is_separete_batteries = module["rectifier"]["userdata"]["is_separete_batteries"]
                dcbara_refv = module["inverter"]["userdata"]["inv_outvoltage"] + 120

                byte_stream = rectifier_common_bytestream(module_id, main_volts_median, main_volts_deviation, 
                    is_separete_batteries, dcbara_refv)

            try:
                crc = Crc16Buypass.calc(byte_stream)
                crc1 = (crc >> 8) & 0x00FF
                crc2 = crc & 0x00FF
                byte_stream.append(crc1)
                byte_stream.append(crc2)
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                ser.write(byte_stream)
                time.sleep(0.01)
                time.sleep(0.3)
                GPIO.output(12, GPIO.HIGH)
                
            except Exception as e:
                task_logger.error("(send_common_user_data) Exception: " + str(e))
                GPIO.output(12, GPIO.HIGH)

    except Exception as e:
        task_logger.error("(send_common_user_data) Exception: " + str(e))
        GPIO.output(12, GPIO.HIGH)


def rectifier_bytestream_individual(module_no, bats_min_dis_vol, bats_float_charge_volt, bat_ah, no_of_bat):

    retVal = [module_no, 16, 0, 43, 0, 4]

    def to_little_endian_float(value):
        """Float değeri little-endian 4 byte olarak döndürür."""
        byte_data = struct.pack('<f', value)
        return [byte_data[3], byte_data[2], byte_data[1], byte_data[0]]

    def to_little_endian_uint16(value):
        """UInt16 değeri little-endian 2 byte olarak döndürür."""
        byte_data = struct.pack('<H', value)
        return [byte_data[1], byte_data[0]]

    retVal.extend(to_little_endian_float(bats_min_dis_vol)) 

    retVal.extend(to_little_endian_float(bats_float_charge_volt)) 

    retVal.extend(to_little_endian_float(bat_ah))


    retVal.extend([0, 0])
    retVal.extend(to_little_endian_uint16(no_of_bat))

    return retVal


def send_individual_user_data(module):
    try:
        moduleType = ["rectifier", "inverter"]
        for m in moduleType:
            if m == "inverter":
                continue
            else:
                module_id = module["module_id"]
                bats_dischargeminvoltage = module["rectifier"]["indiv_userdata"]["bats_dischargeminvoltage"]
                bats_floatchargevoltage = module["rectifier"]["indiv_userdata"]["bats_floatchargevoltage"]
                bats_ah = module["rectifier"]["indiv_userdata"]["bats_ah"]
                bats_numofconnectedbatteries = module["rectifier"]["indiv_userdata"]["bats_numofconnectedbatteries"]

                byte_stream = rectifier_bytestream_individual(module_id, bats_dischargeminvoltage, 
                bats_floatchargevoltage, bats_ah, bats_numofconnectedbatteries)
            
            try:
                crc = Crc16Buypass.calc(byte_stream)
                crc1 = (crc >> 8) & 0x00FF
                crc2 = crc & 0x00FF
                byte_stream.append(crc1)
                byte_stream.append(crc2)
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                ser.write(byte_stream)
                time.sleep(0.01)
                time.sleep(0.3)
                GPIO.output(12, GPIO.HIGH)
                
            except Exception as e:
                task_logger.error("(send_individual_user_data) Exception: " + str(e))
                GPIO.output(12, GPIO.HIGH)

    except Exception as e:
        task_logger.error("(send_individual_user_data) Exception: " + str(e))
        GPIO.output(12, GPIO.HIGH)


def float_to_big_endian_bytes(value):
    """Float değeri big-endian formatında 4 byte olarak döndürür (ModBus standardı)."""
    byte_data = struct.pack('<f', value)  # Little-endian olarak pack et
    return [byte_data[3], byte_data[2], byte_data[1], byte_data[0]]  # Big-endian olarak döndür


def rectifier_calibration_bytestream(module_id, calib_data):

    retVal = [module_id, 16, 0, 58, 0, 15]
    
    def to_little_endian_float(value):
        #Float değeri little-endian 4 byte olarak döndürür.
        byte_data = struct.pack('<f', value)
        return [byte_data[3], byte_data[2], byte_data[1], byte_data[0]]
  
    retVal.extend(to_little_endian_float(calib_data["recti_irms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["recti_irms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["recti_irms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["recti_vrms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["recti_vrms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["recti_vrms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["recti_prms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["recti_prms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["recti_prms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["batt_pos_v"]))
    retVal.extend(to_little_endian_float(calib_data["batt_pos_curr"]))
  
    retVal.extend(to_little_endian_float(calib_data["batt_neg_v"]))
    retVal.extend(to_little_endian_float(calib_data["batt_neg_curr"]))

    retVal.extend(to_little_endian_float(calib_data["dccap_pos_v"]))
    retVal.extend(to_little_endian_float(calib_data["dccap_neg_v"]))

    return retVal


def inverter_calibration_bytestream(module_id, calib_data):

    retVal = [module_id, 16, 0, 81, 0, 27]
    
    def to_little_endian_float(value):
        #Float değeri little-endian 4 byte olarak döndürür.
        byte_data = struct.pack('<f', value)
        return [byte_data[3], byte_data[2], byte_data[1], byte_data[0]]

    retVal.extend(to_little_endian_float(calib_data["bypass_irms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["bypass_irms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["bypass_irms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["bypass_vrms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["bypass_vrms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["bypass_vrms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["bypass_prms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["bypass_prms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["bypass_prms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["inverter_irms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["inverter_irms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["inverter_irms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["inverter_vrms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["inverter_vrms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["inverter_vrms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["inverter_prms_l1"]))
    retVal.extend(to_little_endian_float(calib_data["inverter_prms_l2"]))
    retVal.extend(to_little_endian_float(calib_data["inverter_prms_l3"]))

    retVal.extend(to_little_endian_float(calib_data["load_v_l1"]))
    retVal.extend(to_little_endian_float(calib_data["load_v_l2"]))
    retVal.extend(to_little_endian_float(calib_data["load_v_l3"]))

    retVal.extend(to_little_endian_float(calib_data["inv_v_filt_l1"]))
    retVal.extend(to_little_endian_float(calib_data["inv_v_filt_l2"]))
    retVal.extend(to_little_endian_float(calib_data["inv_v_filt_l3"]))

    retVal.extend(to_little_endian_float(calib_data["inv_mod_sin_l1"]))
    retVal.extend(to_little_endian_float(calib_data["inv_mod_sin_l2"]))
    retVal.extend(to_little_endian_float(calib_data["inv_mod_sin_l3"]))
    
    return retVal


def send_calibration_data(module):
    try:
        moduleType = ["rectifier", "inverter"]
        for m in moduleType:
            if m == "inverter":
                module_id = module["module_id"] + 100
                byte_stream = inverter_calibration_bytestream(module_id, module["inverter"]["calib_data"])
            else:
                module_id = module["module_id"]
                byte_stream = rectifier_calibration_bytestream(module_id, module["rectifier"]["calib_data"])
            try:
                crc = Crc16Buypass.calc(byte_stream)
                crc1 = (crc >> 8) & 0x00FF
                crc2 = crc & 0x00FF
                byte_stream.append(crc1)
                byte_stream.append(crc2)
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                ser.write(byte_stream)
                time.sleep(0.01)
                time.sleep(0.3)
                GPIO.output(12, GPIO.HIGH)
                
            except Exception as e:
                task_logger.error(f"(send_calibration_data) Exception: {e}")
                GPIO.output(12, GPIO.HIGH)

    except Exception as e:
        task_logger.error(f"(send_calibration_data) Exception: {e}")
        GPIO.output(12, GPIO.HIGH)


def send_rtc_update(module):
    """
    Tüm rectifier modüllerine sistem saatini (RTC epoch değeri) gönderir.
    Cihazın saatini güncellemek için kullanılır.
    """
    try:

        module_id = module["module_id"]
        
        # Sistem saatini epoch formatında al (saniye cinsinden Unix timestamp)
        current_epoch = int(time.time())
        
        retVal = [
            module_id,  # Modül ID'si (0-29)
            16,         # Function code: 16 = Write Multiple Registers
            0,          # Start address high byte (88 >> 8 = 0)
            88,         # Start address low byte (register 88'den başla)
            0,          # Register count high byte (2 >> 8 = 0)
            1           # Byte count: 4 byte
        ]
        
        # uint32_t epoch değerini little-endian olarak deneyelim
        # ModBus genelde big-endian ama cihaz little-endian bekliyor olabilir
        epoch_bytes_le = struct.pack('<I', current_epoch)  # Little-endian uint32
        epoch_bytes_be = struct.pack('>I', current_epoch)  # Big-endian uint32
        
        # Little-endian deneyelim
        retVal.extend([epoch_bytes_le[3], epoch_bytes_le[2], epoch_bytes_le[1], epoch_bytes_le[0]])
        
        crc = Crc16Buypass.calc(retVal)
        crc1 = (crc >> 8) & 0x00FF
        crc2 = crc & 0x00FF
        retVal.append(crc1)
        retVal.append(crc2)
        
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        ser.write(bytes(retVal))
        time.sleep(0.01)
        GPIO.output(12, GPIO.LOW)
        time.sleep(0.3)
        GPIO.output(12, GPIO.HIGH)
        
        from datetime import datetime
        current_time_str = datetime.fromtimestamp(current_epoch).strftime('%Y-%m-%d %H:%M:%S')
        
        # Detaylı debug çıktısı
        hex_le = ''.join([f'{b:02X}' for b in epoch_bytes_le])
        hex_be = ''.join([f'{b:02X}' for b in epoch_bytes_be])
        """
        print(f"\n=== RTC Update for Module {module_id} ===")
        print(f"Time: {current_time_str}")
        print(f"Epoch (decimal): {current_epoch}")
        print(f"Epoch (hex): 0x{current_epoch:08X}")
        print(f"Little-endian bytes: {[f'0x{b:02X}' for b in epoch_bytes_le]} = 0x{hex_le}")
        print(f"Big-endian bytes:    {[f'0x{b:02X}' for b in epoch_bytes_be]} = 0x{hex_be}")
        print(f"Sent: Little-endian")
        print(f"Full packet: {[f'0x{b:02X}' for b in retVal]}")
        """
    
    except Exception as e:
        task_logger.error(f"(send_rtc_update) Exception: {e}")
        GPIO.output(12, GPIO.HIGH)


def change_module_id(module):

    try:
        # Inverter ID'si üzerinden komut göndermeliyiz (Master Inverter'dır)
        # Packet Structure: [ID, FunctionCode, Data0, Data1, Data2, Data3]
        byte_stream =[
            module["module_id"] + 100,    # Mevcut Inverter ID (örn: 101)
            0xFF,            #Op code
            0x00,            #Register address high byte
            0x02            # Function Code: 2 (Addressing)
        ]
        
        try:
            def to_little_endian_uint16(value):
                """UInt16 değeri little-endian 2 byte olarak döndürür."""
                byte_data = struct.pack('<H', value)
                return [byte_data[1], byte_data[0]]

            byte_stream.extend(to_little_endian_uint16(module["rectifier"]["header"]["id"] + 100))

            crc = Crc16Buypass.calc(byte_stream)
            crc1 = (crc >> 8) & 0x00FF
            crc2 = crc & 0x00FF
            byte_stream.append(crc1)
            byte_stream.append(crc2)
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            ser.write(byte_stream)
            time.sleep(0.01)
            time.sleep(0.3)
            GPIO.output(12, GPIO.HIGH)
        except Exception as e:
            task_logger.error(f"(change_module_id) Seri Port Hatasi: {e}")
            GPIO.output(12, GPIO.HIGH)
            
    except Exception as e:
        task_logger.error(f"(change_module_id) Veri Hazirlama Hatasi: {e}")
        GPIO.output(12, GPIO.HIGH)


def read_ups():
    try:
        # Web servisi URL'si
        url = "http://localhost:3001/ups-config"
        # GET isteği yap
        response = requests.get(url)

        # Durum kodunu kontrol et
        if response.status_code == 200:
            data = response.json()  # JSON verisini al
            #print("Veri başarıyla alındı:", data)

            return data[0]  # Veriyi döndür
        else:
           print(f"read_ups islem basarili: {response.status_code}")
    except Exception as e:
        periodic_logger.error("read_ups :" + str(e))    


def read_module():
    try:
        """
        ups_data = read_ups()
        if ups_data and "scanAllDevices" in ups_data:
            if ups_data["scanAllDevices"]:
                #for i in range(0, 30):
                for i in range(0, 10):
                    read_module_config.append({
                        "module_id": i,
                        "timestamp":int(time.time() * 1000) - 10 * 60 * 1000,
                        "isSend": True,
                        "dataSend": False
                    })
                return ups_data
        """
        # Web servisi URL'si
        
        url = "http://localhost:3001/module-config"

        # GET isteği yap
        response = requests.get(url)

        # Durum kodunu kontrol et
        if response.status_code == 200:
            
            data = response.json()  # JSON verisini al
            #print("Veri başarıyla alındı:", data)
            
            if selected_module["module_id"] is not None:
                found_module = next((module for module in data if module["module_id"] == selected_module["module_id"] and module["isActive"] == True), None)
            
            for index, element in enumerate(data):
                if not (element["isActive"]):
                    continue
                #element["dataSend"]= True
                read_module_config.append(element)

                if selected_module["module_id"] is not None and found_module is not None and index % 2 == 0 :#and ( dt.now().minute - selected_module["time"]["minutes"]) <= 15  : 
                    #found_module["dataSend"] = True 
                    read_module_config.append(found_module)
            return data

        else:
            print(f"read_module islem basarili: {response.status_code}")
    except Exception as e:
        periodic_logger.error("read_module :" + str(e))    


def write_data(data,endpoint):
    try:
        url = f"http://localhost:3001/{endpoint}"
        # POST isteği gönderimi
        response = requests.post(
            url,
            json=data,  # JSON verisi body'ye ekleniyor
            headers={"Content-Type": "application/json"}  # Header'da içerik tipi belirtiliyor
        )

        # Yanıtın kontrolü
        if response.status_code == 201:
            print(f"write_data islem basarili {response.status_code}")
    except Exception as e:
        periodic_logger.error("write_data : " + str(e))   


def write_task(data, endpoint):
    try:
        url = f"http://localhost:3001/{endpoint}/{str(data['_id'])}"
        # POST isteği gönderimi
        response = requests.patch(
            url,
            json=data,  # JSON verisi body'ye ekleniyor
            headers={"Content-Type": "application/json"}  # Header'da içerik tipi belirtiliyor
        )
        # Yanıtın kontrolü
        if response.status_code == 200:
            print(f"write_task islem basarili {response.status_code}")
    except Exception as e:
        task_logger.info("write_task : " + str(e)) 

def module_task():
    try:
        # Web servisi URL'si
        url = "http://localhost:3001/module-task?status=pending"
        # GET isteği yap
        response = requests.get(url)

        # Durum kodunu kontrol et
        if response.status_code == 200:
            data = response.json()  # JSON verisini al
            #print("Veri başarıyla alındı:", data)
            for task in data:
                try:
                    if task["applyFlags"]["allModules"]:
                        task["module_id"] = 0
                    if task["applyFlags"]["userCmd"]:
                        send_user_commands(task) #Veri gonderiyoruz
                    if task["applyFlags"]["userData"]:
                        send_common_user_data(task) #Tüm modullere Kullanıcı tarafından girilen Verileri gonderiyoruz
                    if task["applyFlags"]["indivUserData"]:
                        send_individual_user_data(task) #Tüm modullere ayrı ayrı Kullanıcı tarafından girilen Verileri gonderiyoruz
                    if task["applyFlags"]["calibData"]:
                        send_calibration_data(task) #Tüm modullere kalibrasyon verilerini gonderiyoruz
                    if task["applyFlags"]["calibCommand"]:
                        send_user_commands(task) #Veri gonderiyoruz
                    if task["applyFlags"]["rtc"]:
                        send_rtc_update(task) #Tüm modullere sistem saatini gonderiyoruz
                    if task.get("applyFlags", {}).get("addressing"):
                        change_module_id(task) # Modül ID adresini değiştiriyoruz (Packet Structure kontrolu gerekli )
                    if task.get("applyFlags", {}).get("firmwareUpgrade"):
                        start_firmware_update(task)
                        
                    for revert in task["revertChanges"]:
                        if revert["status"] == "pending":
                            revert["status"] = "processing"

                except Exception as e:
                    task_logger.error(f"(module_task) Exception: {e}")
                    for revert in task["revertChanges"]:
                        if revert["status"] == "pending":
                            revert["status"] = "failed"
                time.sleep(2)
                write_task(task, "module-task")  # Görevleri güncelle
            return data  # Veriyi döndür
    except Exception as e:
        task_logger.error("module_task :" + str(e))   


def get_value_from_path(data, path):
    """ 'inverter.flags.inv_is_shutdown' → value """
    keys = path.split(".")
    for key in keys:
        data = data.get(key, {})
    return data


def check_module_tasks():
    try:
        # Web servisi URL'si
        url = "http://localhost:3001/module-task?status=processing"
        # GET isteği yap
        response = requests.get(url)

        # Durum kodunu kontrol et
        if response.status_code == 200:
            data = response.json()  # JSON verisini al

            #print("Veri başarıyla alındı:", data)
            for task in data:
                timeout = 40
                
                task_module_id = task["module_id"]

                if task.get("applyFlags", {}).get("addressing"):
                    task_module_id = task["rectifier"]["header"]["id"]

                for change_item in task["revertChanges"] :
                    try:
                        # TASK içinden set_path değeri al
                        status_updated = None
                        task_value = None
                        # Hata takibi için liste
                        failed_module_ids = []
                                
                        # Hangi modülleri kontrol edeceğiz?
                        if task["applyFlags"]["allModules"]:
                            target_modules = check_module_data  # Tüm sistemdeki modüller
                        else:
                            target_modules = [module for module in check_module_data if module["module_id"] == task_module_id]
                        
                        if change_item["type"] == "text":
                            task_value = get_value_from_path(task, change_item["statusPath"]["path"])
                        else :
                            task_value = change_item["statusPath"]["value"]

                        # Tüm hedef modülleri (mesela 30 tanesini) kontrol et
                        for module in target_modules:
                            actual_mod_val = get_value_from_path(module, change_item["statusPath"]["path"])
                            
                            # Eğer değer uyuşmuyorsa, o modülü "hatalılar" listesine at
                            if actual_mod_val != task_value:
                                failed_module_ids.append(module["module_id"])

                        # EĞER LİSTE BOŞ DEĞİLSE
                        if not failed_module_ids:
                            status_updated = "completed"
                        # timeout kontrolü
                        elif int(time.time()) - int(task["time"]) > timeout:
                            status_updated = "failed"
                            change_item["errorModules"] = failed_module_ids
                            change_item["errorDetails"] = "Module(s) did not reach the expected state within the allocated timeout period."

                        else:
                            continue

                        if change_item["status"] != status_updated and status_updated != None :
                            change_item["status"] = status_updated   # Eğer status_updated None ise mevcut status'ü koru    
                            write_task(task, "module-task")  # Her item kontrolünden sonra görevi güncelle
                        time.sleep(0.2)

                    except Exception as e:
                        task_logger.error(f"(module_task) Exception: {e}")
                        change_item["status"] = "failed"
                        change_item["errorModules"] = failed_module_ids
                        change_item["errorDetails"] = f"Task {task['_id']} failed during check_module_tasks. {str(e)}"
                        write_task(task, "module-task")  # Her item kontrolünden sonra görevi güncelle
    except Exception as e:
        task_logger.error("check_pending_tasks : " + str(e))


def create_dfu_packet(module_id, pack_num, begin_req, status_req, packet_send, data_chunk):
    """
    Cihazın (Inverter/Rectifier) beklediği DFU paket yapısını (byte dizisi olarak) oluşturur.
    Bu fonksiyon ModBus altyapısını taklit ederek 0xFF (Özel Komut) OpCode'u ile paketi hazırlar.
    """
    # Header Byte 0: DFU iletişim flag'lerini ve paket numarasını içerir.
    # pack_num (8 bit), begin (1 bit), status (1 bit), send (1 bit), rezerve (5 bit)
    header_val = (pack_num & 0xFF) | ((begin_req & 1) << 8) | ((status_req & 1) << 9) | ((packet_send & 1) << 10)
    
    # ModBus paketinin başlığı: [Hedef Modül ID, 0xFF (OpCode), 0x00, 0x01 (Adres 1 = DFU demek)]
    retVal = [module_id, 0xFF, 0x00, 0x01]
    
    # 16-bit'lik header_val değerini Little-Endian formatında byte'lara ayırır (struct.pack('<H')).
    header_bytes = struct.pack('<H', header_val)
    retVal.extend([header_bytes[0], header_bytes[1]]) # Header'ın ilk 2 byte'ı eklenir.
    retVal.extend([0, 0, 0, 0, 0, 0]) # Header yapısındaki geriye kalan 3 rezerve word (6 byte) boş (0) olarak eklenir.
    
    # Firmware veri parçası 256 word (512 byte) olmalıdır. Eksikse 0xFF ile doldurulur (Flash silinmiş hali).
    padded_chunk = bytearray(data_chunk)
    if len(padded_chunk) < 512:
        padded_chunk.extend(b'\xFF' * (512 - len(padded_chunk)))
    
    # Hazırlanan 512 byte'lık veri bloğu pakete eklenir. Toplam paket uzunluğu artar.
    retVal.extend(padded_chunk)
    
    # Tüm paketin CRC16 hesaplaması yapılır.
    crc = Crc16Buypass.calc(retVal)
    # Hesaplanan CRC, Modbus yapısına uygun olarak High Byte önce, Low Byte sonra olacak şekilde paketin sonuna eklenir.
    retVal.extend([(crc >> 8) & 0xFF, crc & 0xFF])
    return retVal

def parse_dfu_response(raw_data):
    """
    Cihazdan gelen 14 byte'lık DFU durum (status) paketini okur ve anlamlandırır.
    Cihazın hangi pakette olduğunu, başarılı olup olmadığını bu cevaptan anlarız.
    """
    # Gelen veri 14 byte'tan küçükse geçerli bir DFU cevabı değildir.
    if len(raw_data) < 14:
        return None
        
    # CRC Kontrolü: Gelen paketin son 2 byte'ı hariç CRC'sini hesaplarız.
    crc_calc = Crc16Buypass.calc(raw_data[:-2])
    # Cihazın gönderdiği CRC değerini okuruz (High byte ve Low byte).
    crc_read = [raw_data[-2], raw_data[-1]]
    crc_calc_list = [(crc_calc >> 8) & 0x00FF, crc_calc & 0x00FF]
    
    # Eğer CRC uyuşmuyorsa, veri yolda bozulmuştur, paketi geçersiz sayarız.
    if crc_read != crc_calc_list:
        return None
        
    # Data dizisinin 4. byte'ından 12. byte'ına kadar olan 8 byte, DFU_PackInfo_t yapısıdır (Status bilgileri).
    info_bytes = raw_data[4:12]
    # Her iki byte birleşerek 16-bit'lik Word'leri oluşturur (Little-Endian).
    word0 = info_bytes[0] | (info_bytes[1] << 8)
    word1 = info_bytes[2] | (info_bytes[3] << 8)
    word2 = info_bytes[4] | (info_bytes[5] << 8)
    
    # Word'lerdeki bitleri kaydırarak (shift) bayrakların değerlerini okuruz.
    rx_data = {
        "invalid_req": (word0 >> 0) & 1,        # Geçersiz bir istek mi atıldı?
        "erase_failed": (word0 >> 1) & 1,       # Flash silme işlemi başarısız mı oldu?
        "fw_mismatch": (word0 >> 2) & 1,        # Firmware versiyonları uyuşmuyor mu?
        "limp_mode": (word0 >> 3) & 1,          # Sistem güvenli moda mı geçti?
        "key_mismatch": (word0 >> 4) & 1,       # Güvenlik anahtarı (Image Key) hatalı mı?
        "timeout": (word0 >> 5) & 1,            # 60 saniyelik zaman aşımı doldu mu?
        "replied_num": (word0 >> 8) & 0xFF,     # Cihazın cevap verdiği son paket numarası.
        
        "requested_num": (word1 >> 0) & 0xFF,   # Cihazın bizden beklediği BİR SONRAKİ paket numarası.
        "pack_mismatch": (word1 >> 8) & 1,      # Gönderdiğimiz paket no ile beklenen uyuşmadı mı?
        "prog_failed": (word1 >> 9) & 1,        # Paketi flash'a yazarken hata oluştu mu?
        "pack_success": (word1 >> 10) & 1,      # Paket başarıyla yazıldı mı?
        
        "in_progress": (word2 >> 0) & 1,        # DFU işlemi şu an devam ediyor mu?
        "aborted": (word2 >> 1) & 1,            # DFU işlemi iptal mi edildi?
        "reception_completed": (word2 >> 2) & 1,# Tüm paketler (255'e kadar) ulaştı mı?
        "boot_completed": (word2 >> 3) & 1,     # Boot işlemi tamamlandı mı?
    }
    print("rx_data",rx_data)
    return rx_data

def start_firmware_update(task):
    """
    Firmware güncelleme (DFU) işlemini başlatan, yöneten ve sonlandıran ana fonksiyondur.
    Tüm süreç boyunca is_dfu_active bayrağını True tutarak RS485 hattının başka fonksiyonlarca (örn: get_periodic_data) meşgul edilmesini engeller.
    """
    global is_dfu_active
    status_updated = None
    errorDetails= ""

    module_id = task["module_id"]

    moduleType = task["firmwareUpload"]["moduleType"]
    firmwareFile = task["firmwareUpload"]["firmwareFile"]

    is_inverter = (moduleType == "Inverter")
    file_path = os.path.join("..", "api", "src", "uploads", "firmwares", firmwareFile)

    is_dfu_active = True # DFU modunu aktif et, diğer haberleşmeleri durdur.
    
    # Inverter'ların ID'si modül ID'sinin 100 fazlasıdır (örn: 101, 102). Rectifier ise doğrudan (1, 2) ID'ye sahiptir.
    target_id = module_id + 100 if is_inverter else module_id
    
    try:
        
        # Firmware (.bin) dosyasını Binary (byte) formatında okuruz.
        with open(file_path, "rb") as f:
            firmware_data = f.read()
            
        # Cihaz kodunda DFU tamamlanması için KESİNLİKLE 256 paket (128 KB) gönderilmesi bekleniyor (PackNum == 255 kontrolü).
        # Eğer okunan firmware dosyası 128 KB'tan küçükse, sonunu 0xFF (boş flash verisi) ile tamamlamamız GEREKİR. Yoksa cihaz reset atmaz!
        expected_size = 256 * 512 # Toplam 131.072 byte (128 KB)
        if len(firmware_data) < expected_size:
            # Eksik kalan kısmı 0xFF byte'ları ile doldur (Padding)
            firmware_data = firmware_data.ljust(expected_size, b'\xFF')
        elif len(firmware_data) > expected_size:
            status_updated = "failed"
            errorDetails = "Firmware file size exceeds the maximum allowed size."
            is_dfu_active = False
            return False
            
        # Seri portun timeout (zaman aşımı) süresini geçici olarak yedeğe alıyoruz.
        old_timeout = ser.timeout
        # Flash silme işlemi vakit alabileceği için timeout süresini 2.0 saniyeye çıkartıyoruz.
        ser.timeout = 2.0  
            
        # 1. ADIM: BEGIN REQUEST (DFU BAŞLATMA İSTEĞİ GÖNDERİMİ)
        # Data içermeyen, sadece Begin_Req = 1 olan bir tetikleme paketi oluştururuz.
        begin_packet = create_dfu_packet(target_id, pack_num=0, begin_req=1, status_req=0, packet_send=0, data_chunk=b'')
        
        # Seri portta bekleyen çöpleri temizle
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Başlatma paketini RS485 hattına yaz.
        ser.write(bytes(begin_packet))
        time.sleep(0.01)
        GPIO.output(12, GPIO.LOW) # RS485'i okuma moduna çek
        
        # Cihaz flash'ı silip bize cevap (14 byte) dönecektir.
        resp = ser.read(14)
        GPIO.output(12, GPIO.HIGH) # RS485'i tekrar yazma moduna çek
        
        # Gelen cevabı analiz fonksiyonuna gönderiyoruz.
        status = parse_dfu_response(resp)
        
        # Eğer cevap gelmediyse veya cihaz 'in_progress' (işlemde) moduna geçemediyse hata verip iptal ediyoruz.
        if not status or not status["in_progress"]:
            status_updated = "failed"
            errorDetails = "The module did not enter the DFU state or did not respond to the begin request."
            is_dfu_active = False
            ser.timeout = old_timeout
            return False
            
        # Başlangıç başarılı, timeout değerini eski haline getirebiliriz.
        ser.timeout = old_timeout
        
        # 2. ADIM: FIRMWARE PAKETLERİNİN SIRAYLA GÖNDERİLMESİ
        chunk_size = 512 # Cihazın her seferinde beklediği veri büyüklüğü.
        total_chunks = len(firmware_data) // chunk_size # Toplamda kesinlikle 256 çıkmalı (yukarıdaki pad işleminden dolayı)
            
        for i in range(total_chunks):
            # İlgili 512 byte'lık dilimi (chunk) firmware_data içerisinden kesip alıyoruz.
            chunk = firmware_data[i*chunk_size : (i+1)*chunk_size]
            #print(f"Paket gonderiliyor: {i}/{total_chunks-1}...")
            
            # Bu sefer Packet_Send = 1 olan ve Data_Chunk içeren paketi hazırlıyoruz.
            packet = create_dfu_packet(target_id, pack_num=i, begin_req=0, status_req=0, packet_send=1, data_chunk=chunk)
            
            max_retries = 3 # Hataya karşı bir paketi maksimum 3 kere tekrar deneyeceğiz.
            success = False
            
            for r in range(max_retries):
                # Hattı temizle ve paketi yolla
                ser.reset_input_buffer()
                ser.reset_output_buffer()
                ser.write(bytes(packet))
                time.sleep(0.01)
                GPIO.output(12, GPIO.LOW) # Dinleme modu
                
                # Cihaz paketi flasha yazacak, crc doğrulayacak ve bize sonucunu dönecek.
                resp = ser.read(14)
                GPIO.output(12, GPIO.HIGH) # Yazma modu
                
                status = parse_dfu_response(resp)
                
                # Başarı Kontrolü: Cihaz başarılı dönüş yaptıysa ve bizden BİR SONRAKİ paketi (i+1) bekliyorsa işlem tamamdır.
                # (i+1) % 256 yapmamızın sebebi i=255 olduğunda cihazın requested_num olarak 0'a dönebilme ihtimalidir.
                # Ancak son pakette cihaz reception_completed bayrağını yakar ve requested_num'ı arttırmayabilir (255'te kalır).
                if status and status["pack_success"] and (status["requested_num"] == (i + 1) % 256 or status["reception_completed"]):
                    success = True
                    status_updated = "completed"
                    break # Başarılıysa retry döngüsünden çıkıp bir sonraki pakete geç.
                
                # Eğer cihaz programlama hatası (prog_failed) verdiyse:
                elif status and status["prog_failed"]:
                    status_updated = "failed"
                    errorDetails = "Firmware package write failed."
                    time.sleep(0.1)
                
                # Eğer cihaz bizim paket numaramız ile kendi beklediği numaranın uyuşmadığını (mismatch) söylerse:
                elif status and status["pack_mismatch"]:
                    status_updated = "failed"
                    errorDetails = "Firmware package sequence error."
                    time.sleep(0.1)
                
                # Başka bir bilinmeyen hata varsa veya cevap alınamadıysa:
                else:
                    status_updated = "failed"
                    errorDetails = "Unexpected state after firmware package transmission."
                    time.sleep(0.1)
                    
            # 3 denemede de başarılı olamadıysak DFU işlemi iptal olur!
            if not success:
                status_updated = "failed"
                errorDetails = "Failed to send firmware package after multiple attempts."
                is_dfu_active = False
                return False
                
        # Döngü bitti. Eğer son pakete ulaştıysak veya cihaz "Tüm veriyi aldım" (reception_completed) diyorsa işlem bitmiştir.
        if i == total_chunks - 1 or (status and status["reception_completed"]):
            status_updated = "completed"

    except Exception as e:
        # Kodun herhangi bir yerinde Python bazlı bir çökme olursa loglanır.
        status_updated = "failed"
        errorDetails = "Critical error during firmware update."
        task_logger.error(f"(start_firmware_update) Error: {str(e)}")
        return False
    
    finally:
        # İşlem ne olursa olsun (başarılı ya da hata), en sonunda RS485 iletişim pinini standart hale getir
        # ve is_dfu_active bayrağını kapatarak sistemin periyodik sorgulara dönmesini sağla.
        is_dfu_active = False
        GPIO.output(12, GPIO.HIGH)

        for revert in task["revertChanges"]:
            revert["status"] = status_updated
            if status_updated == "failed":
                revert["errorModules"] = [module_id]
                revert["errorDetails"] = errorDetails

        write_task(task, "module-task")  # Her item kontrolünden sonra görevi güncelle
    return True

ser = serial.Serial("COM5", xonxoff=0, rtscts=0)
ser.baudrate = 115200
ser.timeout = 0.5
ser.bytesize = serial.EIGHTBITS
ser.parity = serial.PARITY_NONE
ser.stopbits = serial.STOPBITS_ONE

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(12, GPIO.OUT)
GPIO.output(12, GPIO.HIGH)

def read_msp430_32bit(name, addr, qty=1):
    """
    32-bit Register okur. 
    Cevap formatı: [ID, 03, ByteCount, Data(4*qty), CRC, CRC]
    """
    device_id = 0xF6
    op_code = 0x03
    
    packet = [device_id, op_code, (addr >> 8) & 0xFF, addr & 0xFF, (qty >> 8) & 0xFF, qty & 0xFF]
    crc = Crc16Buypass.calc(packet)
    packet.extend([(crc >> 8) & 0xFF, crc & 0xFF])
    
    # GİDEN VERİYİ BASTIR
    print(f"\nTX {name} (READ): {' '.join(f'{b:02X}' for b in packet)}")
    
    ser.reset_input_buffer()
    GPIO.output(12, GPIO.LOW)
    ser.write(serial.to_bytes(packet))
    ser.flush()
    GPIO.output(12, GPIO.HIGH)
    
    expected_len = 5 + (qty * 4)
    response = ser.read(expected_len)
    
    if len(response) == expected_len:
        # GELEN VERİYİ BASTIR
        print(f"RX {name} (RAW):  {' '.join(f'{b:02X}' for b in response)}")
        
        data_bytes = response[3:-2]
        results = []
        for i in range(qty):
            val = struct.unpack('>I', data_bytes[i*4 : (i+1)*4])[0]
            results.append(val)
        
        print(f"DECODED {name}: {results}")
        return results if qty > 1 else results[0]
    else:
        print(f"!!! {name} HATA: {len(response)} byte geldi (Beklenen: {expected_len})")
        if len(response) > 0:
            print(f"GELEN KISMI VERI: {' '.join(f'{b:02X}' for b in response)}")
        return None
def write_msp430_relay(val):
    """
    Röle yazma (8 Byte Fixed format)
    Echo: Gönderilen paketin aynısı
    """
    device_id = 0xF6
    op_code = 0x10
    addr = 0x0002
    
    packet = [device_id, op_code, (addr >> 8) & 0xFF, addr & 0xFF, (val >> 8) & 0xFF, val & 0xFF]
    crc = Crc16Buypass.calc(packet)
    packet.extend([(crc >> 8) & 0xFF, crc & 0xFF])
    
    # GİDEN VERİYİ BASTIR
    print(f"\nTX WRITE (RELAY): {' '.join(f'{b:02X}' for b in packet)}")
    
    ser.reset_input_buffer()
    GPIO.output(12, GPIO.LOW)
    ser.write(serial.to_bytes(packet))
    ser.flush()
    GPIO.output(12, GPIO.HIGH)
    
    response = ser.read(8)
    if len(response) == 8:
        # GELEN VERİYİ BASTIR
        print(f"RX ECHO (RELAY):  {' '.join(f'{b:02X}' for b in response)}")
        return True
    else:
        print(f"!!! WRITE HATA: {len(response)} byte geldi")
        return False

try:
    print("\n" + "="*60)
    print("   MSP430 32-BIT MODBUS RTU KAPSAMLI TEST")
    print("="*60)
    while True:
        # 1. Sensörleri Oku
        print("\n--- SENSÖR VE GİRİŞ OKUMALARI ---")
        read_msp430_32bit("DIJITAL_GIRISLER", 3)
        read_msp430_32bit("SICAKLIK_ADC", 4)
        
        # 2. TEKER TEKER AÇ/KAPA TESTİ
        print("\n--- RÖLE TESTİ: TEKER TEKER ---")
        for i in range(5):
            mask = (1 << i)
            print(f">> Röle-{i+1} Açılıyor (Mask: {mask:02X})...")
            if write_msp430_relay(mask):
                time.sleep(0.5)
                read_msp430_32bit(f"TEYİT_ROLE_{i+1}", 1)
                
                # Hemen kapat
                write_msp430_relay(0)
                time.sleep(0.2)
        # 3. TOPLU AÇ/KAPA TESTİ
        print("\n--- RÖLE TESTİ: HEPSİ BİRDEN ---")
        print(">> TÜM RÖLELER AÇILIYOR...")
        if write_msp430_relay(31): # 0x1F = Tüm 5 röle
            time.sleep(2.0)
            read_msp430_32bit("TEYİT_HEPSİ_ACIK", 1)
        print(">> TÜM RÖLELER KAPATILIYOR...")
        if write_msp430_relay(0):
            time.sleep(1.0)
            read_msp430_32bit("TEYİT_HEPSİ_KAPALI", 1)
        print("\n" + "#"*40)
        print("Tüm testler başarıyla tamamlandı. 5 sn bekliyor...")
        print("#"*40)
        time.sleep(5)


except Exception as e:
    print(f"HATA OLUŞTU: {e}")
    task_logger.error("MSP430 Test Error: " + str(e))
finally:
    ser.close()
    GPIO.cleanup()
