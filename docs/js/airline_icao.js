/*
 * Syrup Firmware — ICAO airline designator → name (ADS-B callsign prefix).
 * Incomplete by design; unknown codes fall back to the 3-letter code.
 */
(function (global) {
  'use strict';

  /** @type {Record<string, {zh: string, en: string}>} */
  var AIRLINES = {
    // China
    CCA: { zh: '中国国际航空', en: 'Air China' },
    CSN: { zh: '中国南方航空', en: 'China Southern' },
    CES: { zh: '中国东方航空', en: 'China Eastern' },
    CHH: { zh: '海南航空', en: 'Hainan Airlines' },
    CSC: { zh: '四川航空', en: 'Sichuan Airlines' },
    CXA: { zh: '厦门航空', en: 'XiamenAir' },
    CQH: { zh: '春秋航空', en: 'Spring Airlines' },
    CDC: { zh: '成都航空', en: 'Chengdu Airlines' },
    CDG: { zh: '山东航空', en: 'Shandong Airlines' },
    CQN: { zh: '重庆航空', en: 'Chongqing Airlines' },
    CSH: { zh: '上海航空', en: 'Shanghai Airlines' },
    CSZ: { zh: '深圳航空', en: 'Shenzhen Airlines' },
    CTJ: { zh: '天津航空', en: 'Tianjin Airlines' },
    CUH: { zh: '乌鲁木齐航空', en: 'Urumqi Air' },
    CUA: { zh: '中国联合航空', en: 'China United' },
    CBJ: { zh: '首都航空', en: 'Beijing Capital' },
    CBD: { zh: '长龙航空', en: 'Loong Air' },
    CGZ: { zh: '多彩贵州航空', en: 'Colorful Guizhou' },
    CGN: { zh: '桂林航空', en: 'Guilin Airlines' },
    CJX: { zh: '江西航空', en: 'Jiangxi Air' },
    CQM: { zh: '青岛航空', en: 'Qingdao Airlines' },
    CSS: { zh: '中国货运航空', en: 'China Cargo' },
    CKK: { zh: '中国货运邮政', en: 'China Postal' },
    CYZ: { zh: '中国货运', en: 'China Postal Airlines' },
    EPA: { zh: '东海航空', en: 'Donghai Airlines' },
    DKH: { zh: '吉祥航空', en: 'Juneyao Air' },
    GCR: { zh: '天津航空', en: 'Tianjin Airlines' },
    HXA: { zh: '华夏航空', en: 'China Express' },
    HYN: { zh: '湖南航空', en: 'Air Travel' },
    JYH: { zh: '九元航空', en: '9 Air' },
    KNA: { zh: '昆明航空', en: 'Kunming Airlines' },
    LKE: { zh: '幸运航空', en: 'Lucky Air' },
    MBA: { zh: '北部湾航空', en: 'GX Airlines' },
    OTQ: { zh: '奥凯航空', en: 'Okay Airways' },
    QDA: { zh: '青岛航空', en: 'Qingdao Airlines' },
    RBW: { zh: '瑞丽航空', en: 'Ruili Airlines' },
    SNG: { zh: '四川航空', en: 'Sichuan Airlines' },
    TBA: { zh: '西藏航空', en: 'Tibet Airlines' },
    UEA: { zh: '成都航空', en: 'Chengdu Airlines' },
    WOW: { zh: '西部航空', en: 'West Air' },
    CHB: { zh: '西部航空', en: 'West Air' },
    CGH: { zh: '彩虹无人机', en: 'Caihong UAV' },

    // Greater China / regional
    CPA: { zh: '国泰航空', en: 'Cathay Pacific' },
    HDA: { zh: '香港快运', en: 'HK Express' },
    CRK: { zh: '香港航空', en: 'Hong Kong Airlines' },
    HKE: { zh: '香港快运', en: 'HK Express' },
    AHK: { zh: '港龙货运', en: 'Air Hong Kong' },
    AMU: { zh: '澳门航空', en: 'Air Macau' },
    CAL: { zh: '中华航空', en: 'China Airlines' },
    EVA: { zh: '长荣航空', en: 'EVA Air' },
    MDA: { zh: '华信航空', en: 'Mandarin Airlines' },
    TNA: { zh: '复兴航空', en: 'UNI Air' },

    // Major international (common over China / hubs)
    AFL: { zh: '俄罗斯航空', en: 'Aeroflot' },
    AFR: { zh: '法国航空', en: 'Air France' },
    AAL: { zh: '美国航空', en: 'American Airlines' },
    ACA: { zh: '加拿大航空', en: 'Air Canada' },
    ANA: { zh: '全日空', en: 'All Nippon Airways' },
    ASA: { zh: '阿拉斯加航空', en: 'Alaska Airlines' },
    BAW: { zh: '英国航空', en: 'British Airways' },
    CPA: { zh: '国泰航空', en: 'Cathay Pacific' },
    DAL: { zh: '达美航空', en: 'Delta Air Lines' },
    DLH: { zh: '汉莎航空', en: 'Lufthansa' },
    ETD: { zh: '阿提哈德', en: 'Etihad Airways' },
    EZY: { zh: '易捷航空', en: 'easyJet' },
    FDX: { zh: '联邦快递', en: 'FedEx' },
    FIN: { zh: '芬兰航空', en: 'Finnair' },
    GFA: { zh: '海湾航空', en: 'Gulf Air' },
    HAL: { zh: '夏威夷航空', en: 'Hawaiian Airlines' },
    IBE: { zh: '西班牙国家航空', en: 'Iberia' },
    JAL: { zh: '日本航空', en: 'Japan Airlines' },
    JBU: { zh: '捷蓝航空', en: 'JetBlue' },
    KAL: { zh: '大韩航空', en: 'Korean Air' },
    KLM: { zh: '荷兰皇家航空', en: 'KLM' },
    MAS: { zh: '马来西亚航空', en: 'Malaysia Airlines' },
    MDA: { zh: '华信航空', en: 'Mandarin Airlines' },
    QFA: { zh: '澳洲航空', en: 'Qantas' },
    QTR: { zh: '卡塔尔航空', en: 'Qatar Airways' },
    RYR: { zh: '瑞安航空', en: 'Ryanair' },
    SAS: { zh: '北欧航空', en: 'SAS' },
    SIA: { zh: '新加坡航空', en: 'Singapore Airlines' },
    SWA: { zh: '西南航空', en: 'Southwest Airlines' },
    THA: { zh: '泰国国际航空', en: 'Thai Airways' },
    THY: { zh: '土耳其航空', en: 'Turkish Airlines' },
    UAL: { zh: '联合航空', en: 'United Airlines' },
    UAE: { zh: '阿联酋航空', en: 'Emirates' },
    UPS: { zh: 'UPS', en: 'UPS' },
    VIV: { zh: '墨西哥 Viva', en: 'VivaAerobus' },
    VOI: { zh: '墨西哥 Volaris', en: 'Volaris' },
    AAR: { zh: '韩亚航空', en: 'Asiana Airlines' },
    APJ: { zh: '乐桃航空', en: 'Peach Aviation' },
    JJP: { zh: '捷星日本', en: 'Jetstar Japan' },
    AJX: { zh: '全日空翼', en: 'Air Japan' },
    AXM: { zh: '亚洲航空', en: 'AirAsia' },
    TGW: { zh: '酷航', en: 'Scoot' },
    SEJ: { zh: 'SpiceJet', en: 'SpiceJet' },
    GOW: { zh: 'Go First', en: 'Go First' },
    IGO: { zh: 'IndiGo', en: 'IndiGo' },
    AIC: { zh: '印度航空', en: 'Air India' },
    SVA: { zh: '沙特航空', en: 'Saudia' },
    ETH: { zh: '埃塞俄比亚航空', en: 'Ethiopian Airlines' },
    RZO: { zh: '俄罗斯皇家', en: 'Royal Flight' },
    SBI: { zh: '西伯利亚航空', en: 'S7 Airlines' },
    SVR: { zh: '乌拉尔航空', en: 'Ural Airlines' },
    AZO: { zh: '阿塞拜疆航空', en: 'Azerbaijan Airlines' },
    MSR: { zh: '埃及航空', en: 'EgyptAir' },
    RAM: { zh: '摩洛哥皇家航空', en: 'Royal Air Maroc' },
    TAP: { zh: '葡萄牙航空', en: 'TAP Air Portugal' },
    AUA: { zh: '奥地利航空', en: 'Austrian Airlines' },
    SWR: { zh: '瑞士国际航空', en: 'SWISS' },
    BEL: { zh: '布鲁塞尔航空', en: 'Brussels Airlines' },
    LOT: { zh: '波兰航空', en: 'LOT Polish' },
    AEE: { zh: '爱琴海航空', en: 'Aegean Airlines' },
    TRA: { zh: 'Transavia', en: 'Transavia' },
    WZZ: { zh: '威兹航空', en: 'Wizz Air' },
    NAX: { zh: '挪威航空', en: 'Norwegian' },
    ICE: { zh: '冰岛航空', en: 'Icelandair' },
    VIR: { zh: '维珍航空', en: 'Virgin Atlantic' },
    VLG: { zh: '伏林航空', en: 'Vueling' },
    EWG: { zh: '欧洲之翼', en: 'Eurowings' },
    CFG: { zh: '康多尔航空', en: 'Condor' },
    TOM: { zh: 'TUI 航空', en: 'TUI Airways' },
    AZA: { zh: '意大利航空', en: 'ITA Airways' },
    ITY: { zh: '意大利航空', en: 'ITA Airways' },
    GEC: { zh: '汉莎货运', en: 'Lufthansa Cargo' },
    CLX: { zh: '卢森堡货运', en: 'Cargolux' },
    GTID: { zh: 'Atlas Air', en: 'Atlas Air' },
    GTI: { zh: 'Atlas Air', en: 'Atlas Air' },
    CKK: { zh: '中国货运邮政', en: 'China Postal Airlines' }
  };

  // Fix accidental 4-letter key
  delete AIRLINES.GTID;

  /**
   * Parse ADS-B callsign into airline + flight number.
   * Typical form: CCA1558, CSN6350A (optional letter suffix).
   */
  function parseCallsign(raw) {
    var callsign = String(raw || '').replace(/\s+/g, '').toUpperCase();
    var empty = {
      callsign: callsign || '',
      airlineCode: '',
      flightNumber: '',
      flightId: '',
      airlineName: '',
      known: false
    };
    if (!callsign) return empty;

    var m = callsign.match(/^([A-Z]{3})(\d{1,4}[A-Z]?)$/);
    if (!m) {
      empty.callsign = callsign;
      return empty;
    }

    var code = m[1];
    var num = m[2];
    var info = AIRLINES[code];
    var lang = 'zh';
    try {
      if (typeof global.getCurrentLang === 'function') {
        lang = global.getCurrentLang() === 'en' ? 'en' : 'zh';
      }
    } catch (e) {}

    var name = '';
    if (info) {
      name = lang === 'en' ? info.en : info.zh;
    }

    return {
      callsign: callsign,
      airlineCode: code,
      flightNumber: num,
      flightId: code + num,
      airlineName: name || code,
      known: !!info
    };
  }

  function airlineNameForCode(code) {
    var c = String(code || '').toUpperCase();
    var info = AIRLINES[c];
    if (!info) return c || '';
    var lang = 'zh';
    try {
      if (typeof global.getCurrentLang === 'function') {
        lang = global.getCurrentLang() === 'en' ? 'en' : 'zh';
      }
    } catch (e) {}
    return lang === 'en' ? info.en : info.zh;
  }

  global.SyrupAirline = {
    parseCallsign: parseCallsign,
    airlineNameForCode: airlineNameForCode,
    AIRLINES: AIRLINES
  };
})(typeof window !== 'undefined' ? window : this);
