const express = require('express');
const http = require('http');
const { APIError, parseSkipLimit } = require('../helpers');
const {ResourceManagerModel, DeviceMapModel, DeviceSpecificationModel, RecipeModel, DeviceGroupModel} = require('../models');
const {PointModel, DeviceModel, PayloadModel, DecisionModel, CommunicationInterfaceModel} = require('../models');
const {MicroControlerModel, OperationProfileModel, MeasurementModel, EventModel } = require('../models');
const opts = {
  "opt": {
    "tasks": {
      "supply": "supply"
    },
    "specs": {
      "flow": "flowmeter",
      "valve": "relay_valve"
    },
    "communication": {
      "ttsIp": "172.21.0.10",
      "ttsBearer": "",
      "mode": {
        "payloadType": "text",
        "flags": {
          "operation_profile": "o",
          "action": "a",
          "report": "r",
          "sync": "s",
          "forward": "f",
          "continue": "c",
          "single": "s",
          "batch": "b",
          "time": "n",
          "instruction": "i",
          "measurement": "m",
          "event": "e",
          "trigger": "t",
          "date": "d",
          "init": "0",
          "ping": "1",
          "on": "1",
          "off": "0",
          "standard_block_separator": "_",
          "single_block_separator": "s",
          "value_separator": ";"
        },
        "edge":{
          "maxPayloadSize": "50",
          "reportsDecimalPlaces":"2",
          "minimumUplinkInterval": "60",
          "live": "60000",
          "initRetry": "60",
          "defaultUplinkInterval": "10",
          "actionFaultThreshold": "120",
          "defaultDevGroup":"d"
        }
      }
    }
  },
  "setup": {
    "supply": {
      "in": {
        "flow": {
          "mSec": 5,
          "uSec": 30
        }
      },
      "out": {
        "valve": {
        }
      }
    }
  }
};

async function readDeviceGroups(request, response, next) {
  let skip = parseSkipLimit(request.query.skip) || 0;
  let limit = parseSkipLimit(request.query.limit, 1000) || 1000;
  if (skip instanceof APIError) {
    return next(skip);
  } else if (limit instanceof APIError) {
    return next(limit);
  }
  try {
    const deviceGroups = await DeviceGroupModel.readDeviceGroups({}, {}, skip, limit);
    return response.json(deviceGroups);
  } catch (err) {
    return next(err);
  }
}

async function readDeviceSpecifications(request, response, next) {
  let skip = parseSkipLimit(request.query.skip) || 0;
  let limit = parseSkipLimit(request.query.limit, 1000) || 1000;
  if (skip instanceof APIError) {
    return next(skip);
  } else if (limit instanceof APIError) {
    return next(limit);
  }

  try {
    const deviceSpecifications = await DeviceSpecificationModel.readDeviceSpecifications({}, {}, skip, limit);
    return response.json(deviceSpecifications);
  } catch (err) {
    return next(err);
  }
}

// Ingest data at a single point in time from an end-to-end physical field device.
class Measurement{

  constructor(id, vl, tm, dv, pid, spec) {
    this.id = id || '_' + Math.random().toString(36).substr(2, 9);
    this.vl = vl;
    this.tm = tm;
    this.dv = dv || {};
    this.pid = pid;
    this.spec = spec || null;
  }

  static async which (id) {
    const measurement = await MeasurementModel.readMeasurement(id);
    return measurement;
  }

  static async that (object) {
    let {id, vl, tm, dv, pid, spec} = object;
    await MeasurementModel.genMeasurement(new MeasurementModel({id:id,vl:vl,tm:tm,dvid:dv.id,pid:pid, spec:spec}))
  }

  async over (opts) {
    const { id } = opts;
    await MeasurementModel.updateMeasurement(opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await MeasurementModel.deleteMeasurement(id);
  }

  static async DBfind (query, fields) {
    return await MeasurementModel.readMeasurements(query, {}, 0, 1000).then(
      (data) => {
        return Object.values(data).map((d) => {
          let {id, vl, tm, dvid, pid, spec} = d;
          return new Measurement(id, vl, tm, dvid, pid, spec);
        })
      }
    );
  }

};

// Represents a geographic point of interest in the cloud. It could be a station
// soil, crop, field, water collector, water course, water resource. The water resource
// directly connected to the water paths directly connected to the collectors. Collectors must
// include at least one field that should include one or more
// different crops which could include ground stations.
class Point{

  constructor(id, kind, legend, loc, root, parent, cldrn, c, ctrl) {
    this.id = id || '_' + Math.random().toString(36).substr(2, 9);
    this.kind = kind;
    this.legend = legend;
    this.loc = loc || {};
    this.root = root || {};
    this.parent = parent || {};
    this.cldrn = cldrn || [];
  }

  static async which (id) {
    const {kind, legend, loc,root_point_id,parent_id,children_ids,interface_id,controler_id} = await PointModel.readPoint(id);
    return new Point(id, kind, legend,loc,root_point_id,parent_id,children_ids,interface_id,controler_id); // TODO see what to do with these ids
  }

  static async that (opts) {
    let point = await PointModel.genPoint(new PointModel(opts));
    return point;
  }

  async over (opts) {
    const { id } = opts;
    await PointModel.updatePoint(opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await PointModel.deletePoint(id);
  }

};

// Visualization of a physical microprocessor (eg Raspberry Pi) placed at a spatial point
// which guides based on the software it runs, the communication interface, any sensors
// or actuators that are connected and any subnets.
class MicroControler{

  constructor(id, att, microControlers, communicationInterface, devs, operationProfile, point, devGroup, on, live, dep, sg) {
    this.id = id || '_' + Math.random().toString(36).substr(2, 9);
    this.att = att || null;
    this.ctrls = microControlers || [];
    this.c = communicationInterface || {};
    this.devs = devs || {};
    this.op = operationProfile;
    this.point = point || {};
    this.devGroup = devGroup || {};
    this.on = on || false;
    this.live = live || false;
    this.dep = dep;
    this.sg = sg || null;
    this.pmcid = null;
  }

  async setOp(op){
    this.op = op;
    return this.over({id:this.id, oid:this.op.id});
  }

  async setSNet(ctrls, save){
    this.ctrls = {};
    ctrls.forEach((ctrl)=>{
      this.ctrls[ctrl.id] = ctrl;
    })
    if(save){
      this.over({id:this.id, mcids:ctrls.map((ctrl) => ctrl.id)});
    }
  }

  islive(){
    this.live = true;
  }

  isdead(){
    this.live = false;
  }

  static async which (id) {
    const {att, cid, dev_ids, mcids, oid, pid, dev_group, on, live, dep, sg} = await MicroControlerModel.readMicroControler(id);
    return new MicroControler(id, att, mcids, cid, dev_ids, oid, pid, dev_group, on, live, dep, sg);
  }

  static async that (opts) {
    opts.id = '_' + Math.random().toString(36).substr(2, 9); // because when called in All promise making the same id
    let mcModel = await MicroControlerModel.genMicroControler(new MicroControlerModel(opts));
    const {id, att, cid, dev_ids, mcids, oid, pid, dev_group, on, live, dep, sg} = mcModel;
    return new MicroControler(id, att, mcids, cid, dev_ids, oid, pid, dev_group, on, live, dep, sg);
  }

  async over (opts) {
    const { id } = opts;
    return await MicroControlerModel.updateMicroControler(id, opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await MicroControlerModel.deleteMicroControler(id);
  }
};

// It represents the physical communication interface of each point responsible for communication
// with cloud computing through the LoRaWAN network layer or to communicate with one another
// subnet.
class CommunicationInterface{
  constructor(id, mode, spectrum, netId, kind, q, mcid) {
    this.id = id;
    this.mode = mode;
    this.spectrum = spectrum;
    this.netId = netId;
    this.kind = kind;
    this.q = q || [];
    this.mcid = mcid || null;
  }

  shift(number, fromTime, toTime){
    const _this = this;
    if(fromTime, toTime){
      let counter = 0;
      let elements = [];
      for(let i = 0 ; i < _this.q.length ; i++){
        let e = _this.q[i];
        if(e.tm > fromTime && e.tm <= toTime && counter < number){
          elements.push(e);
          _this.q.splice(i,1);
          counter++;
          i--;
        }
      }
      return elements;
    }
    return this.q.splice(0, number);
  }

  empty(){
    this.q = [];
  }

  length(){
    return this.q.length > 0;
  }

  static async which (id) {
    const {mode, spectrum, nid, kind, q} = await CommunicationInterfaceModel.readCommunicationInterface(id);
    return new CommunicationInterface(id, mode, spectrum, nid, kind, q);
  }

  static async that (opts) {
      let c = await CommunicationInterfaceModel.genCommunicationInterface(new CommunicationInterfaceModel(opts));
      return c;
  }

  async over (opts) {
    const { id } = opts;
    return await CommunicationInterfaceModel.updateCommunicationInterface(opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await CommunicationInterfaceModel.deleteCommunicationInterface(id);
  }

};

// Represents an uplink or downlink load of the LoRaWAN server.
class Payload{
  constructor(id, payload, tm, c, kind, act) {
    this.id = id;
    this.payload = payload || {};
    this.tm = tm;
    this.c = c || {};
    this.kind = kind;
    this.act = act;
  }

  static async which (id) {
    const payload = await PayloadModel.readPayload(id);
    return payload;
  }

  static async that (object) {
    object['cid'] = object.c.id;
    delete object.c;
    await PayloadModel.genPayload(new PayloadModel(object))
  }

  async over (opts) {
    const { id } = opts;
    await PayloadModel.updatePayload(opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await PayloadModel.deletePayload(id);
  }

};


class Downlink extends Payload {

  constructor(id, payload, tm, c, act){
    super(id, payload, tm, c, 'downlink', act);
  }
};


class Uplink extends Payload {

  constructor(id, payload, tm, c, data, act, details){
    super(id, payload, tm, c, 'uplink', act);
    this.data = data;
    this.f_cnt = details.f_cnt;
    this.f_port = details.f_port;
    this.frm_payload = details.frm_payload;
    this.gw_id = details.gw_id;
    this.rssi = details.rssi;
    this.snr = details.snr;
    this.channel_index = details.channel_index;
    this.channel_rssi = details.channel_rssi;
    this.spreading_factor = details.spreading_factor;
    this.bandwidth = details.bandwidth;
    this.data_rate_index = details.data_rate_index;
    this.coding_rate = details.coding_rate;
    this.frequency = details.frequency;
    this.toa = details.toa;
  }

};


class Join extends Payload {

  constructor(id, tm, c){
    super(id, null, tm, c, 'join');
  }

};

// It is reflected in an end device receiving data or implementing actions that works
// based on device model specifications and instructions given by the computer
// cloud on the microprocessor that drives it.
class Device{
  constructor(id, legend, spec, att, mcid, io) {
    this.id = id || '_' + Math.random().toString(36).substr(2, 9);
    this.legend = legend || '';
    this.spec = spec;
    this.att = att || '';
    this.mcid = mcid || '';
    this.io = io || '';
  }

  static async which (id) {
    const {legend, sid, att, mc_id, io} = await DeviceModel.readDevice(id);
    return new Device(id, legend, sid, att, mc_id, io);
  }

  static async that (opts) {
    if(!opts.id){
      opts.id = '_' + Math.random().toString(36).substr(2, 9);
    }
    return await DeviceModel.genDevice(new DeviceModel(opts))
  }

  async over (opts) {
    const { id } = opts;
    await DeviceModel.updateDevice(opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await DeviceModel.deleteDevice(id);
  }

};


class Actuator extends Device {
  constructor(id, legend, spec, att, mcid, io){
    super(id, legend, spec, att, mcid, io);
    this.att = 'out';
  }
};


class Sensor extends Device {

  constructor(id, legend, spec, att, mcid, io){
    super(id, legend, spec, att, mcid, io);
    this.att = 'in';
  }
};


class OperationProfile{

  constructor(id, mcid, mAtt, cAtt, dAtt, nets) {
    this.id = id || '_' + Math.random().toString(36).substr(2, 9);
    this.mcid = mcid;
    this.ctrls = mAtt || {};
    this.cis = cAtt || {};
    this.devs = dAtt || {};
    this.nets = nets || {};
  }

static async which (id) {
  const {mc_id, ctrls, cis, devs, nets} = await OperationProfileModel.readOperationProfile(id);
  return new OperationProfile(id, mc_id, ctrls, cis, devs, nets);
}

async that (opts) {
  await OperationProfileModel.genOperationProfile(new OperationProfileModel(opts))
}

async over (opts) {
  const { id } = opts;
  await OperationProfileModel.updateOperationProfile(opts);
}

async nomore (opts) {
  const { id } = opts;
  await OperationProfileModel.deleteOperationProfile(id);
}
};

// Event that occurred at a single time from a final physical field device.
class Event{

  constructor(vl, tm, dv, aid, pid) {
    this.id = '_' + Math.random().toString(36).substr(2, 9);
    this.vl = vl;
    this.tm = tm;
    this.dv = dv || {};
    this.aid = aid || null;
    this.pid = pid;
    this.circ = 'wait';
  }

  static async which (id) {
    const evnt = await EventModel.readEvent(id);
    return evnt;
  }

  static async that (object) {
    const {id,vl,tm,dv,circ} = object;
    await EventModel.genEvent(new EventModel({id:id,vl:vl,tm:tm,dvid:dv.id,circ:circ}))
  }

  async over (opts) {
    const { id } = opts;
    await EventModel.updateEvent(opts);
  }

  async nomore (opts) {
    const { id } = opts;
    await EventModel.deleteEvent(id);
  }

};


class Decision{

  constructor(devs, input) {
    this.id = '_' + Math.random().toString(36).substr(2, 9);
    this.devs = devs || [];
    this.model_id = input;
  }


static async which (id) {
  const decision = await DecisionModel.readDecision(id);
  return decision;
}

async that (opts) {
  await DecisionModel.genDecision(new DecisionModel(opts))
}

async over (opts) {
  const { id } = opts;
  await DBDecisionModel.updateDecision(opts);
}

async nomore (opts) {
  const { id } = opts;
  await DBDecisionModel.deleteDecision(id);
}
};

// It contains an action that must occur on a field end device and is a decision product
// received from the decision processing unit.
class Action extends Decision {
  constructor(devs, kind, input, tm){
    super(devs, input);
    this.kind = kind || {};
    this.tm = tm || null;
  }
};

class NetworkGateway {
  constructor(app) {
    this.app = app;
  }
  hear(orchestrator, systemCallback) {
    const _this = this;
    const bodyParser = require("body-parser")
    _this.app.use(bodyParser.json())
    _this.app.post("/uplinks", (req, res) => {
      try{systemCallback.apply(orchestrator,[req.body,'uplink']);}catch(e){console.log(e)}
    });
    _this.app.post("/joins", (req, res) => {
      try{systemCallback.apply(orchestrator,[req.body,'join']);}catch(e){console.log(e)}
    });
    _this.app.post("/downlinks/sent", (req, res) => {
    });
    _this.app.post("/downlinks/line", (req, res) => {
    });
    _this.app.post("/downlinks/join", (req, res) => {
    });
  }
};

class rq {

  constructor(app, orchestrator) {
    this.app = app;
    this.o = orchestrator;
  }

  init(){
    const _this = this;
    const orchestrator = _this.o;
    _this.handlers = {
      rm_data: orchestrator.getManager,
      rm_gen: orchestrator.createManager,
      rm_add: orchestrator.genManager,
      points_add: orchestrator.genPoints,
      point_add: orchestrator.genSolePoint,
      mcs_add: orchestrator.getCtrls,
      mc_add: orchestrator.getCtrl,
      mc_data: orchestrator.getTopCtrlDevInfo,
      dev_data: orchestrator.getDevInfo,
      devs_data: orchestrator.getDevsInfo,
      actions_add: orchestrator.genCmds,
      action_add: orchestrator.genCmd,
      dev_groups: orchestrator.openDeviceGroups,
      dev_specs: orchestrator.openDeviceSpecifications,
      payloads: orchestrator.openPayloads,
      op_setup: orchestrator.openSetup,
      sys_config: orchestrator.openOpts
    }
    return _this.handlers;
  }

  atReq(fn){
    const _this = this;
    return async function (request, response, next) {
      let skip = parseSkipLimit(request.query.skip) || 0;
      let limit = parseSkipLimit(request.query.limit, 1000) || 1000;
      if (skip instanceof APIError) {
        return next(skip);
      } else if (limit instanceof APIError) {
        return next(limit);
      }

      try {
        let opts = Object.values(request.opts);
        let result = await fn.apply(_this.o, opts);
        if(result.data){
          return response.json(result.data);
        }
        return response.json(result);
      } catch (err) {
        console.log(err)
        return next(err);
      }
    }
  }
};

class APIRouter {

  constructor(app, orchestrator, rq) {
    this.app = app;
    this.o = orchestrator;
    this.rq = rq;
    this.links = {
        '/api/dv-groups':[
          {param:'',fn:'dev_groups'}
        ],
        '/api/dv-specs':[
          {param:'',fn:'dev_specs'}
        ],
        '/api/operation-setup':[
          {param:'',fn:'op_setup'}
        ],
        '/api/system-configuration':[
          {param:'',fn:'sys_config'},
          {param:'/:cmd',fn:'sys_config'}
        ],
        '/api/rm_data':[
          {param:'/rmId=:rmId',fn: 'rm_data'}
        ],
        '/api/mc_data':[
          {param:'/rmId=:rmId&mcid=:mcid&from=:from&to=:to',fn: 'mc_data'}
        ],
        '/api/mc_all_data':[
          {param:'/:rmId&:mcid',fn: 'mc_data'}
        ],
        '/api/dev_data':[
          {param:'/rmId=:rmId&devId=:devId&from=:from&to=:to',fn: 'dev_data'}
        ],
        '/api/payloads':[
          {param:'',fn: 'payloads'},
        ],
        '/api/point_add':[
          {param:'/rmId=:rmId&legend=:legend&kind=:kind&lat=:lat&lon=:lon',fn: 'point_add'}
        ],
        '/api/mc_add':[
          {param:'/rmId=:rmId&att=:att&pointId=:pointId&devGroup=:devGroup&ciId=:ciId&on=:on',fn: 'mc_add'}
        ],
        '/api/action_add':[
          {param:'/rmId=:rmId&devId=:devId&vl=:vl&tm=:tm',fn: 'action_add'}
        ],
      }
  }

  initLinks(){
    const _this = this;
    Object.keys(_this.links).forEach((path)=>{
      let br = _this.links[path];
      _this.genLink(path, br);
    })
  }

  genLink(path, a){
    const router = new express.Router();
    a.forEach((handler)=>{
      let fn = this.rq.atReq(this.rq.handlers[handler.fn]);
      router.link(handler.param).get(fn)
    })
    this.app.use(path, router);
  }

};

class ModelAccessor{

  constructor(){
  }

  async openManagers(){
    return await ResourceManagerModel.readResourceManagers({}, {}, 0, 1000);
  }

  async genManager(data){
    return await ResourceManagerModel.genManager(new ResourceManagerModel(data));
  }

  async openDeviceSpecifications(){
    let devSpecs = await DeviceSpecificationModel.readDeviceSpecifications({}, {}, 0, 1000);
    return devSpecs;
  }

  async openDeviceGroups(){
    let devGroups = await DeviceGroupModel.readDeviceGroups({}, {}, 0, 1000);
    return devGroups;
  }

  async openPayloads(){
    let payloads = await PayloadModel.readPayloads({}, {}, 0, 1000);
    return payloads;
  }

  async openDeviceSpecification(id){
    return await DeviceSpecificationModel.readDeviceSpecification(id);
  }

  async openDeviceMap(id){
    return await DeviceMapModel.readDeviceMap(id);
  }

  async genDeviceMap(data){
    return await DeviceMapModel.genDeviceMap(new DeviceMapModel(data));
  }

  async genPoint(data){
    return await Point.that(data);
  }

  async genMicroControler(data){
    return await MicroControler.that(data);
  }

  async openRecipe(id){
    return await RecipeModel.readRecipe(id);
  }

  async openRecipes(){
    return await RecipeModel.readRecipes({}, {}, 0, 1000);
  }

  async openCommunicationinterfaces(){
    return await CommunicationInterfaceModel.readCommunicationInterfaces({}, {}, 0, 1000);
  }

  async openDefs(){
    return Promise.all([
      ModelAccessor.prototype.openRecipes(),
      ModelAccessor.prototype.openDeviceGroups(),
      ModelAccessor.prototype.openCommunicationinterfaces()
    ]);
  }

  openOpts(cmd){
    if(cmd == 'mode'){
      return opts.opt.communication.mode;
    }
    return opts.opt;
  }

  openSetup(){
    return opts.setup;
  }

};

// Resource managers are functionally independent systems and support the management of a virtual
// subnet of IoT devices in cloud computing. Each running manager is root in
// IoT application layer and corresponds to a user or operator specific subsystem with personalized
// configuration, operation settings and spatial data. The resource manager additionally uses
// separated service for loading database models (Model Accessor),
// including the device map which includes the end devices of the IoT network
// and the operating scenario which is a specification map containing end-user preferences
// and/or decision models.
class ResourceManager {
  constructor(orchestrator, id, nid, dmid, points, sopt, setup) {
    this.o = orchestrator;
    this.id = id;
    this.nid = nid;
    this.deviceMap = dmid;
    this.points = points || [];
    this.sopt = sopt;
    this.setup = setup;
  }

  setSystemConfig(config) {
    this.sopt = config;
  }

  readOpts() {
    return this.sopt;
  }

  setOperationSetup(setup) {
    this.setup = setup;
  }

  readSetup() {
    return this.setup;
  }

  async openData() {
    this.deviceMap = (typeof this.deviceMap == 'string') ? await this.o.openDeviceMap(this.deviceMap) : {};
    this.points = await Promise.all(this.points.map(async (pid)=>{
      return await Point.which(pid);
    }))
    return await Promise.all([this.deviceMap].concat(this.points))
  }

};

class Payloader{

  constructor(settings, env) {
    this.settings = settings || {};
    this.env = env;
  }

  writeMessage(cmd, data, op){
    let payload = {};
    if(this.settings.payloadType === 'text'){
      if(cmd == 'action'){
        payload.text = this.settings.flags[cmd] + this.writeActions(data);
      }
      else if(cmd == 'continue'){
        payload.text = this.settings.flags[cmd];
      }
      else {
        console.error('ERROR', 'Failed');
      }
    }
    return payload;
  }

  writeEpoch(tm){
    return (tm) ? Math.round(tm / 1000) : '';
  }

  writeActions(data){
    const _this = this;
    let {events, kind, enumMethod, isPart, tm} = data;
    let parts = [];
    events.forEach((n)=>{
      let devicePart = [];
      let dev = n.dv;
      let eventValue = _this.settings.flags[n.vl];
      let actionType = _this.settings.flags[kind];
      let deviceIndex = data.index;
      let routePart = _this.buildRoutePart([deviceIndex]);
      devicePart = devicePart.concat([_this.settings.flags['single']],routePart);
      let valuePart = _this.buildValuePart([eventValue], actionType);
      devicePart = devicePart.concat(valuePart);
      let timePart = _this.buildTimePart(tm, n.tm);
      devicePart = devicePart.concat([_this.settings.flags['date']],timePart);
      parts = parts.concat(devicePart);
    })
    if(isPart){
      parts.push(_this.settings.flags.continue)
    }
    if(tm){
      parts.unshift(_this.buildTimePart(tm));
    }
    return parts.join('');
  }


  read(payload, op){
    const _this = this;
    if(_this.settings.payloadType === 'text'){
      let blocks = [];
      let isPart = false;
      let msgParts = [];
      let cmd = payload[0];
      let commandStr = Object.keys(_this.settings.flags).find(fl => this.settings.flags[fl] === cmd);
      payload = payload.substr(1);
      if(payload.length == 0){
        console.error('WARNING', 'There is only the initial flag on the payload.', payload);
        return {blocks:blocks, cmd:commandStr , isPart:isPart};
      }
      isPart = payload[payload.length - 1] == this.settings.flags['continue'];
      if(isPart){
        payload = payload.slice(0, -1);
      }
      if(payload[0] == this.settings.flags['forward']){
        payload = payload.substr(1);
        blocks = blocks.concat(this.readParts(cmd, payload ,this.settings.flags.single_block_separator));
      } else {
        msgParts = payload.split(this.settings.flags['forward']).filter(Boolean);
        let separator = (cmd == this.settings.flags['action']) ? this.settings.flags.single_block_separator : this.settings.flags.standard_block_separator;
        blocks = blocks.concat(this.readParts(cmd, msgParts[0] ,separator));
        if(msgParts[1]){
          blocks = blocks.concat(this.readParts(cmd, msgParts[1] ,this.settings.flags.single_block_separator));
        }
      }
      if(_this.env === 'cloud'){
        blocks.forEach(d => {
          d[0] = op.cis.num[d[0]]
        })
      }
      return {blocks:blocks, cmd:commandStr , isPart:isPart};
    }
  }

  readParts(cmd, payload, separator){
    let blocks = [];
    let parts = payload.split(separator).filter(Boolean);
    let epoch = null;
    if(parts[0].length == 10){
      epoch = parts[0];
      parts.shift();
    }
    for (let i = 0; i < parts.length; i++) {
      let part = parts[i];
      let devicePart = this.readDevicePart(part, cmd, epoch);
      if(devicePart){
        blocks.push(devicePart);
      }
    }
    return blocks;
  }

  readDevicePart(part, cmd, epoch){
    let contents = [];
    let routePart, valuePart = [];
    if(cmd==this.settings.flags['action']){
      contents = part.split(this.settings.flags['trigger']);
      routePart = contents[0];
      if(contents[1].includes(this.settings.flags['date'])){
        let subContents = contents[1].split(this.settings.flags['date']);
        valuePart[0] = subContents[0];
        valuePart[1] = this.readTime(epoch, subContents[1]);
      } else {
        valuePart = contents.slice(1);
      }
    }
    if(cmd==this.settings.flags['operation_profile']){
      if (part.includes(this.settings.flags['instruction'])) {
        contents = part.split(this.settings.flags['instruction']);
        routePart = contents[0];
        valuePart = contents.slice(1);
      } else {
        routePart = part;
      }
    }
    if(cmd==this.settings.flags['report']){
      let reportFlags = this.settings.flags['measurement'] + this.settings.flags['event'];
      contents = part.split(part.match(new RegExp("([\d.]+)?[" + reportFlags + "]","g"))[0]);
      routePart = contents[0];
      if(contents[1].includes(this.settings.flags['date'])){
        let subContents = contents[1].split(this.settings.flags['date']);
        valuePart[0] = subContents[0];
        valuePart[1] = this.readTime(epoch, subContents[1]);
      } else {
        valuePart = contents.slice(1);
      }
    }
    return [routePart,valuePart];
  }

  readTime(epoch, diff){
    if(diff){
      let diffSecondsTime = Number(diff) * 60;
      let toSecondsTime = Number(epoch) + diffSecondsTime;
      return toSecondsTime * 1000;
    }
    return Number(epoch) * 1000;
  }

  buildValuePart(values, flag){
    let part = [];
    for (let i = 0 ; i < values.length ; i++){
      if(flag){
        part.push(flag);
      }
      part.push(values[i]);
    }
    return part;
  }

  buildRoutePart(ids){
    let part = [];
    for (let i = 0 ; i < ids.length ; i++){
      part.push(ids[i]);
      if(i < ids.length - 1){
        part.push('_');
      }
    }
    return part;
  }

  buildTimePart(epochTime, toTime){
    let epoch = Math.round(epochTime / 1000);
    if(toTime){
      let toSecondsTime = Math.round(toTime / 1000);
      let diffSecondsTime = toSecondsTime - epoch;
      let diff = Math.round(diffSecondsTime / 60);
      return diff;
    }
    return epoch;
  }

};

class IORouter {

  constructor(manager) {
    this.manager = manager;
    this.payloader = new Payloader(manager.sopt.communication.mode, 'cloud');
  }

  execute(cmd, c, Rctrl){
    const _this = this;
    Rctrl = Rctrl || _this.getCtrl(c.mcid);
    let cmdsPerMsgLimit = 999;
    if(_this.payloader.settings.payloadType === "text"){
      cmdsPerMsgLimit = 6;
    }
    if(c.length()){
      let fromTime, toTime = null;
      if(cmd == 'action'){
        fromTime = new Date();
        fromTime.setHours(0, 0, 0, 0);
        toTime = new Date();
        toTime.setDate(toTime.getDate() + 1);
        toTime.setHours(23, 59, 59, 999);
        fromTime = fromTime.getTime();
        toTime = toTime.getTime();
      }
      let items = c.shift(cmdsPerMsgLimit, fromTime, toTime);
      if(items.length > 0){
        let isPart = (cmdsPerMsgLimit - items.length) == 0 && c.q.length > 0;
        let tm = Date.now();
        let payload = {};
        if (cmd == 'action') {
            let data = {events: items, kind:'trigger', enumMethod: 'single', isPart: isPart, tm:tm};
            payload = this.payloader.writeMessage(cmd, data, Rctrl.op);
        }
        let peer = Rctrl.c;
        let id = peer.id + '_' + tm;
        let downlink = new Downlink(id, payload, tm, peer);
        this.send.call(this, downlink);
      }
    }
  }

  send(downlink, forceDownlink) {
    if(this.payloader.settings.payloadType === 'text' && downlink.payload.text){
      downlink.payload = Buffer.from(downlink.payload.text).toString('base64'); // str payload in base64 format
    }
    let payload = forceDownlink || {
      appId: this.manager.nid,
      devId: downlink.c.id,
      frmPayload: downlink.payload
    };
    let url = "/api/v3/as/applications/"+payload.appId+"/webhooks/payloads/devs/"+payload.devId+"/down/push";
    let data = '{"downlinks":[{"frm_payload":"'+ payload.frmPayload +'","f_port":41,"priority":"NORMAL"}]}';
    const options = {
        hostname: this.config.communication.ttsIp,
        port:1885,
        path: url,
        method: 'POST',
        headers: {
          'Authorization': this.config.communication.ttsBearer,
        }
    };
    const req = http.request(options, (res) => {
        res.on('data', (chunk) => {
            console.log(`BODY: ${chunk}`);
        });
        res.on('end', () => {
          Downlink.that(downlink);
        });
    });
    req.on('error', (e) => {
        console.error(`problem: ${e.payload}`);
    });
    req.write(data);
    req.end();
  }


};

// The device manager is the core service of the application because it integrates the end devices that
// are in the field including their physical capabilities at the software level
// creating a virtual ecosystem between end users and remote end devices
// (IoT). In this way the workflow and the relationships between the end devices are integrated into the
// cloud computing. Also the device manager takes over all communication procedures and
// acts as a data input/output router to and from end devices by exchanging
// messages with the LoRaWAN server.
class DeviceControler extends IORouter {

  constructor(manager) {
    super(manager);
    this.config = manager.readOpts() || {};
    this.setup = manager.readSetup() || {};
    this.deviceMap = manager.deviceMap || {};
    this.tree = {};
    this.ctrls = [];
    this.interfaces = [];
    this.specs = {};
    this.devGroups = {};
    this.devs = [];
  }

  async openData() {
    return await this.manager.o.openDeviceSpecifications().then(async(specs) => {
      this.specs = specs;
      return await this.manager.o.openDeviceGroups().then(async(devGroups) => {
        this.devGroups = devGroups;
        return await this.initializations();
      });
    });
  }

  async initializations(deviceMap){
    const _this = this;
    deviceMap = deviceMap || this.deviceMap;
    await Promise.all(deviceMap.nodes.map(async (node) => {
      const rmcId = node.rmc_id;
      await _this.openCtrlContents(rmcId, true).then(async(Rctrl) => {
        return Rctrl;
      }).then((Rctrl)=>{
        _this.tree[Rctrl.id] = Rctrl;
        return Rctrl;
      })
    })).then(()=>{
      return _this.tree;
    });
  }

  async openCtrlContents(i, onlyActives) {
    const _this = this;
    return await MicroControler.which(i).then(async (ctrl) => {
      if (onlyActives && !ctrl.on) {
        return ctrl;
      }
      _this.ctrls.push(ctrl);
      const ciId = ctrl.c;
      return await CommunicationInterface.which(ciId).then(async (c) => {
        c.mcid = ctrl.id;
        ctrl.c = c;
        _this.interfaces.push(c);
        ctrl.devGroup = _this.devGroups.find(devGroup => devGroup.id === ctrl.devGroup);
        if(ctrl.devs.length == 0 && ctrl.devGroup){
          return await _this.genDevs(ctrl.devGroup, i).then(async(devIds) => {
            ctrl.devs = devIds;
            return await ctrl.over({id:ctrl.id, dev_ids: devIds}).then(async () => {
              return await _this.openDevs(ctrl.devs, ctrl.id).then((devs)=>{
                ctrl.devs = devs;
                return ctrl;
              });
            })
          });

        } else if(ctrl.devs.length > 0){
          return await _this.openDevs(ctrl.devs, ctrl.id).then((devs)=>{
            ctrl.devs = devs;
            return ctrl;
          })
        }
      });
  });
}

  async openDevs(f, c) {
    const _this = this;
    const devs = {};
    return await Promise.all(f.map(async (devId) => {
      return await Device.which(devId).then(async (e) => {
        let specId = e.spec;
        let pe = _this.specs.find(s => s.id == specId);
        if (e.att === 'in') {
          e = new Sensor(e.id, e.legend, pe, e.att, c, e.io);
        } else if (e.att === 'out') {
          e = new Actuator(e.id, e.legend, pe, e.att, c, e.io);
        } else if (e.att === 'forward') {
          e = new Device(e.id, e.legend, pe, e.att, c, e.io);
        }
        devs[e.id] = e;
        _this.devs[e.id] = e;
        return e;
      });
    }))
    .then(async(devs) => {
      return devs;
    });
  }

  async genDevs(devGroup, mcid) {
    const _this = this;
    let l = devGroup.devs;
    return await Promise.all(Object.keys(l).map(async (io) => {
      let kind = l[io];
      let pe = this.specs.find(s => s.kind == kind);
      let opts = {legend:kind, sid:pe.id, att:pe.att, mc_id:mcid, io:io};
      return await Device.that(opts).then(async (dev) => {
        return dev.id;
      });
    }));
  }

  command(a, action) {
    const _this = this;
    let cFull = {};
    a.forEach((e)=>{
      let mcid = e.dv.mcid;
      const Rctrl = _this.getTopCtrl(mcid);
      let rci = Rctrl.c;
      let line = rci.q;
      let j = 0;
      while ( j < rci.q.length && rci.q[j].tm < item.tm ) {j ++};
      line.splice(j, 0, item);
      rci.q = line;
      cFull[rci.id] = rci;
    })

    Object.keys(cFull).forEach((ciid)=>{
      let c = cFull[ciid];
      let kind = c.kind;
      const Rctrl = _this.getTopCtrl(c.mcid);
      if(kind == 'C'){
        _this.execute('action', c, Rctrl);
      }
    })

  }

  async genCtrls(s){
    const _this = this;
    return await Promise.all(s.map(async (p) => {
      return await MicroControler.that(p).then((ctrl)=>{
        return ctrl;
      })
    }));
  }

  getDev(dv) {
    if(!dv.id){
      return this.devs[dv];
    }
    return dv;
  }

  getDevs(devs, att, kind) {
    devs = devs.map(dev => this.getDev(dev));
    if(att){
      devs = devs.filter((d)=>{
        return d.att == att;
      })
    }
    if(kind){
      devs = devs.filter((d)=>{
        let specs = this.config.specs;
        const devType = Object.keys(specs).find(key => specs[key] == d.spec.kind);
        return devType == kind;
      })
    }
    return devs;
  }

  getCtrlDevs(ctrl, att, kind) {
    return this.getDevs(ctrl.devs, att, kind);
  }

  getCtrlDDevs(ctrl, att, kind) {
    let sdevs = [];
    let subMcs = (ctrl.ctrls.length) ? ctrl.ctrls.length : Object.values(ctrl.ctrls);
    subMcs.forEach((mmc)=>{
      let mmcDevs = this.getDevs(mmc.devs, att, kind);
      sdevs.concat(mmcDevs);
    })
    return sdevs;
  }

  getADevs(att, kind) {
    return this.getDevs(this.devs, att, kind);
  }

  getDevIds(devs){
    return devs.map((d => d.id))
  }

  getDevBy(Rctrl, io){
    if(io[0] === 'f'){
      let meshNodeId = io.substr(1);
      let mmc = Object.values(Rctrl.ctrls).find(mmc => mmc.sg === meshNodeId);
      return Object.values(mmc.devs)[0].id;
    } else {
      return Rctrl.devs.find(d => d.io === io.toString()).id;
    }
  }

  getCtrl(mcid) {
    const _this = this;
    let ctrl = _this.ctrls.find(ctrl => ctrl.id == mcid);
    return ctrl;
  }

  getTopCtrl(mcid) {
    const _this = this;
    let rmcId = null;
    if(_this.tree[mcid]){
      return _this.tree[mcid];
    }
    else {
      let ctrl = _this.ctrls.find(ctrl => ctrl.id == mcid);
      return _this.getCtrl(ctrl.pmcid);
    }
  }

};

// Service that configures the operating instructions of the end devices, which are thus not responsible
// to know exactly what they should do. Feature composition includes profiling
// operation based on system and device specification maps, settings
// operation and operation scenarios.
class OperationComposer{
  constructor(manager) {
    this.manager = manager;
    this.config = manager.readOpts() || {};
    this.setup = manager.readSetup() || {};
    this.fs = {};
  }

  async buildFs(l){
    return await Promise.all(l.map(async (m) => {
      return await this.buildF(m);
    }))
  }

  async buildF(c){
      if (!c.op){
        let p = this.compF(c);
        c.setOp(p);
        return await p.that(p).then(()=> {
          return c;
        })
      } else if (typeof c.op === 'string' || c.op instanceof String){
        return await OperationProfile.which(c.op).then(async (p) => {
          c.setOp(p);
          return c;
        });
      }
  }

  compF(ctrl) {
    const _this = this;
    const config = _this.config;
    const setup = _this.setup;
    let p = {};
    p['ctrls'] = {};
    p['cis'] = {};
    p['nets'] = {
      num: [],
      devs: {}
    };
    p.cis['cid'] = ctrl.c.id;
    p.cis['num'] = [];
    p['devs'] = {};
    Object.keys(ctrl.devs).forEach((d)=>{
      let dev = ctrl.devs[d];
      p.cis.num.push(dev.id);
      let specs = config.specs;
      let dt = Object.keys(specs).find((k) => {
        return specs[k] === dev.spec.kind;
      });
      let task = ctrl.devGroup.task;
      p.devs[dev.id] = {};
      p.devs[dev.id]['io'] = dev.io;
      p.devs[dev.id][dev.att] = setup[task][dev.att][dt];
    });
    p.cis['options'] = {};
    p.cis.options['interval'] = 60
    let { ctrls, cis, devs, nets } = p;
    let i = new OperationProfile(null, ctrl.id, ctrls, cis, devs, nets);
    this.fs[i.id]=i;
    return i;
};

}

// Service that interacts with database storage. Handles the modification of
// data coming from the communication with the field and stores or serves them.
class DataHandler {
  constructor(manager) {
    this.manager = manager;
  }

  async readData(dataClass, query) {
    return await dataClass.DBfind(query).then((results)=>{
        return results;
    });
  }

  encode(ids, from, to){
    if(!ids){
      return {};
    }
    let ids = ids.map((id) => { return {dvid:id}});
    from = from || 0;
    to = to || Date.now()
    return {
      $and: [{
          $or: ids
        },
        {
          tm: {
            $gt: from
          }
        },
        {
          tm: {
            $lt: to
          }
        }
      ]
    }
  }

};

// Decision Support System (DSS) which operates based on incoming data and
// management models assigned to each resource manager.
class DecisionProcessor{

  constructor(manager) {
    this.manager = manager;
  }

  genCmd(ct, map, e, ou, t){
    if(ct == 'action'){
      let pos = t || Date.now();
      let act = new Action(map, 'trigger', ou, pos);
      let es = map.map((dev) => new Event(e, pos, dev, act.id));
      let cFull = {};
      es.forEach((e)=>{
        let mcid = e.dv.mcid;
        const Rctrl = _this.getTopCtrl(mcid);
        let rci = Rctrl.c;
        let line = rci.q;
        let j = 0;
        while ( j < rci.q.length && rci.q[j].tm < item.tm ) {j ++};
        line.splice(j, 0, item);
        rci.q = line;
        cFull[rci.id] = rci;
      })

      Object.keys(cFull).forEach((ciid)=>{
        let c = cFull[ciid];
        let kind = c.kind;
        const Rctrl = _this.getTopCtrl(c.mcid);
        if(kind == 'C'){
          _this.execute('action', c, Rctrl);
        }
      })

    }
  }

};

// The core of cloud computing consists of an Internet of Things orchestrator that
// it virtualizes the network devices, manages the data they produce or receives, and implements it
// communication protocol. More specifically, the orchestrator can simultaneously support the
// operation of many different IoT and data management subsystems, different scenarios and
// type of users, incorporating personalized parameters and models.
class Orchestrator extends ModelAccessor{

  constructor(app) {
    super();
    this.app = app;
    this.networkGateway = new NetworkGateway(app);
    this.resourceManagers = {};
  }

  async init() {
    const _this = this;
    _this.openManagers().then(async (dv) => {
      if (dv.length == 0) {
        throw new Error(' No managers');
        return;
      }
      await Promise.all(dv.map(async (i) => {await _this.start(i)})).then(()=>{
        console.log(' Ready. ')
      })
    });
    _this.networkGateway.hear(_this, _this.trancive);
    _this.rq = new rq(_this.app, _this);
    const handlers = _this.rq.init();
    _this.APIRouter = new APIRouter(_this.app, _this, _this.rq);
    _this.APIRouter.initLinks();
  }

  async start(resourceManagerModel) {
    const _this = this;
    const {
      id,
      nid,
      dmid,
      point_ids,
    } = resourceManagerModel;

    let sopt = _this.openOpts();
    let setup = _this.openSetup();

    const manager = new ResourceManager(_this, id, nid, dmid, point_ids, sopt, setup);
    return await manager.openData().then(async () => {
      manager.operationComposer = new OperationComposer(manager);
      manager.deviceControler = new DeviceControler(manager);
      await manager.deviceControler.openData().then(async () => {
        let activeRmcs = manager.deviceControler.ctrls.filter(ctrl => ctrl.att === 'Rctrl').filter(ctrl => ctrl.on == true);
        await manager.operationComposer.buildFs(activeRmcs).then(() => {
          manager.dataHandler = new DataHandler(manager);
          manager.decisionProcessor = new DecisionProcessor(manager);
           console.log('ResourceManager', manager.id, 'is ready.')
          return manager;
        });
        return manager;
      });
      _this.resourceManagers[id] = manager;
      return manager;
    });
  }

  trancive(payload, kind) {
    const _this = this;
    const managers = Object.values(this.resourceManagers).find((manager) => {return manager.nid == payload.end_device_ids.application_ids.application_id})
    const id = payload.correlation_ids[0].replace(/[as:up]/g, '');
    try{
      const c = managers.deviceControler.interfaces.find((i) => i.id == payload.end_device_ids.dvid);
      if(!c){
              throw new Error('No communication interface with id '+payload.end_device_ids.dvid+ ' exists.');
      }
      let Rctrl = managers.deviceControler.getTopCtrl(c.mcid);
      const tm = new Date(payload.received_at).getTime();
      if(kind == 'uplink'){
        let raw = payload.uplink_message.readd_payload.raw;
        const b64 = payload.uplink_message.frm_payload;
        let details = {
          f_cnt: payload.uplink_message.f_cnt,
          f_port: payload.uplink_message.f_port,
          frm_payload: payload.uplink_message.frm_payload,
          gw_id: payload.uplink_message.rx_metadata[0].gateway_ids.gateway_id,
          rssi: payload.uplink_message.rx_metadata[0].rssi,
          snr: payload.uplink_message.rx_metadata[0].snr,
          channel_index: payload.uplink_message.rx_metadata[0].channel_index,
          channel_rssi: payload.uplink_message.rx_metadata[0].channel_rssi,
          spreading_factor: payload.uplink_message.settings.data_rate.lora.spreading_factor,
          bandwidth: payload.uplink_message.settings.data_rate.lora.bandwidth,
          data_rate_index: payload.uplink_message.settings.data_rate_index,
          coding_rate: payload.uplink_message.settings.coding_rate,
          frequency: payload.uplink_message.settings.frequency,
          toa: null,
        }

        let uplink = null;
        if(Rctrl.devGroup.legacy){
          if(!Rctrl.op){
            managers.operationComposer.buildF(Rctrl);
          }

          raw = ['r', Math.round(tm / 1000), '_'].concat(raw.split('_').filter(part => part != '').map((v, i) => {
            return i+'m'+v;
          }).join('_')).join('');

        }

        let Rctrl = _this.getTopCtrl(c.mcid);
        let uplinkData = null;
        if(_this.payloader.settings.payloadType === 'text'){
          uplinkData = _this.payloader.read(raw, Rctrl.op);
        }
        let uplink = new Uplink(id, raw, time, c, uplinkData.blocks, uplinkData.cmd, details);
        Uplink.that(uplink);
        if(uplink.act == 'continue'){
          managers.deviceControler.execute('operation_profile', Rctrl.c, Rctrl);
        }
        if(uplink.act == 'report'){
          let t = tm;
          let relation = uplink.data;
          let data = [];
          relation.forEach((ud) => {
            if(!ud[0]){
              return;
            }
            let dev = this.manager.deviceControler.getDev(ud[0]);
            let dd = ud[1][0];
            if(ud[1][0].split){
              dd = ud[1][0].split(';');
            }
            for (let i=0 ; i < dd.length ; i++){
              let specific = dev.spec.store[i];
              let co = dd[i];
              t = ud[1][1] || t;
              let d = null;
              if (dev instanceof Sensor) {
                d = new Measurement(null, co, t, dev, uplink.id, specific);
              }
              else if (dev instanceof Actuator) {
                d = new Event(co, t, dev, null, uplink.id, specific);
              }
              data.push(d);
            }
          })
          data.forEach((d) => {
            if (d instanceof Measurement) {
              Measurement.that(d);
            }
            else if (d instanceof Event) {
              Event.that(d);
            }
          });
          managers.deviceControler.execute('action', c, Rctrl);
        }
      }
      else if (kind == 'join'){
          let Rctrl = _this.getTopCtrl(c.mcid);
          let join = new Join(id, tm, c, details);
          Join.that(join);
      }
    }catch(e){console.log(e)}
  }

  async createManager(user) {
    const _this = this;
    const sopt = _this.openOpts();
    return await _this.openDefs().then((data) => {
      let options = {
        options:{
            tasks:Object.keys(sopt.tasks),
            location_types:["collector", "link", "resource"],
            specs: sopt.specs,
            operation:{
              minimumUplinkInterval: sopt.communication.mode.edge.minimumUplinkInterval
            }
        },
        device_groups: data[1],
        communication_interfaces: data[2].filter((c)=>c.mode == 'lora'),
        setup: _this.openSetup()
      };
      return {data: {options:options }};
    });
  }

  async genManager(options) {
    const _this = this;
    const {points, device_maps, resource_managers, setup} = options;
    let tmp = {};
    let ps = [];
    await Promise.all(points.map(async (p) => {
      let tmp_id = p.id;
      delete p.id;
      p.id = '_' + Math.random().toString(36).substr(2, 9);
      let pointModel = await _this.genPoint(p);
      tmp[tmp_id] = pointModel.id;
      ps.push(pointModel.id);
    }));
    let tmpd = {};
    await Promise.all(device_maps.map(async (dm) => {
      let rmcs = [];
      await Promise.all(dm.nodes.map(async (ctrl) => {
        ctrl.pid = tmp[ctrl.pid];
        ctrl.on = true;
        let microControlerModel = await _this.genMicroControler(ctrl);
        rmcs.push({
          rmc_id: microControlerModel.id
        });
      }));
      dm.nodes = rmcs;
      let tmp_id = dm.id;
      delete dm.id;
      let deviceMapModel = await _this.genDeviceMap(dm);
      tmpd[tmp_id] = deviceMapModel.id;
    }));
    let tmpm = {};
    let managers = await Promise.all(resource_managers.map(async (manager) => {
      let tmp_id = manager.id;
      delete manager.id;
      manager.dmid = tmpd[manager.dmid];
      manager.point_ids = ps;
      let resourceManagerModel = await _this.genManager(manager);
      tmpm[tmp_id] = resourceManagerModel.id;
      return await _this.start(resourceManagerModel)
    }));
    return {
      manager: managers[0],
      data: {
        id: managers[0].id,
        tree: managers[0].deviceControler.tree,
        points: managers[0].points
      }
    }
  }

  getManager(rmId){
    let manager = this.resourceManagers[rmId];
    return {
      manager: manager,
      data: {
        id: manager.id,
        tree: manager.deviceControler.tree,
        points: manager.points
      }
    }
  }

  async getTopCtrlDevInfo(rmId, rmcId, from, to){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    let Rctrl = manager.deviceControler.getTopCtrl(rmcId);
    let devIds = Rctrl.devs.map((d) => d.id);
    return _this.getDevsInfo(rmId, devIds, from, to);
  }

  async getDevsInfo(rmId, devIds, from, to){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    let query = manager.dataHandler.encode(devIds, from, to);
    let measurements = await manager.dataHandler.readData(Measurement, query);
    let events = await manager.dataHandler.readData(Event, query);
    return {data: {measurements: measurements, events: events}};
  }

  async getDevInfo(rmId, devId, from, to){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    let query = manager.dataHandler.encode([devId], from, to);
    let measurements = await manager.dataHandler.readData(Measurement, query);
    let events = await manager.dataHandler.readData(Event, query);
    return {data: {measurements: measurements, events: events}};
  }

  async getCtrls(rmId, nodes){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    return await manager.deviceControler.genCtrls(nodes).then(async (ctrls)=>{
      let newMcs = ctrls.map((ctrl) => {return {rmc_id:ctrl.id,mmc_ids:ctrl.ctrls.map(ctrl=>ctrl.id)}});
      manager.deviceControler.deviceMap.nodes = manager.deviceControler.deviceMap.nodes.concat(newMcs);
      return await manager.deviceControler.initializations({nodes:newMcs}).then(()=>{
        return {data: {tree: manager.deviceControler.tree}};
      });
    })
  }

  async getCtrl(rmId, att, pointId, devGroup, ciId, on){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    let nodes = [{att: att, pid:pointId, dev_group:devGroup, cid: ciId, on: on}];
    return await manager.deviceControler.genCtrls(nodes).then(async (ctrls)=>{
      let newMcs = ctrls.map((ctrl) => {return {rmc_id:ctrl.id,mmc_ids:ctrl.ctrls.map(ctrl=>ctrl.id)}});
      manager.deviceControler.deviceMap.nodes = manager.deviceControler.deviceMap.nodes.concat(newMcs);
      return await manager.deviceControler.initializations({nodes:newMcs}).then(()=>{
        return {data: {tree: manager.deviceControler.tree}};
      });
    })
  }

  async genCmds(rmId, commands){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    commands.forEach((c) => {
      let devId = c[0];
      let vl = c[1];
      let tm = c[2];
      manager.decisionProcessor.genCmd('action', [
        manager.deviceControler.devs[devId]
      ], vl, 'manual', tm)
    });
    return {data: {done:1}}
  }

  async genCmd(rmId, devId, vl, tm){
    const _this = this;
    let manager = _this.getManager(rmId).manager;
    tm = tm || Date.now();
    manager.decisionProcessor.genCmd('action', [
      manager.deviceControler.devs[devId]
    ], vl, 'manual', tm)
    return {data: {done:1}}
  }

  async genPoints(rmId, nodes){
    return {
      data: {
        points: await Promise.all(nodes.map(async (pointModelParams) => {
          return await Point.that(pointModelParams).then((point) => {
            return point;
          })
        }))
      }
    }
  }

  async genSolePoint(rmId, legend, kind, lat, lon) {
    return {
      data: {
        point: await Point.that({
          legend: legend,
          kind: kind,
          loc: {
            coordinates: [lat, lon],
            kind: "Point"
          }
        }).then((point) => {
          return point;
        })
      }
    }
  }
}

let middleware = new Orchestrator();

middleware.init();
