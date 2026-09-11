"""Terrain-aware motion, independent of rendering and hardware.

Horizontal coordinates are local east/north metres. Altitude is ellipsoid
height minus the fixed launch ellipsoid height, never clearance above terrain.
"""
from dataclasses import replace
import math
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'shared/python'))
from arrietty_geo.common import geoid_offset, number
from arrietty_up import constants as c


class TerrainUnavailable(ValueError):
    pass


class HeightPatch:
    def __init__(self, data):
        try:
            self.e=float(data['east']);self.n=float(data['north']);self.step=float(data['step'])
            self.count=data['count'];self.heights=data['heights']
            if type(self.count) is not int or not 3<=self.count<=25 or not 1<=self.step<=20:
                raise ValueError()
            if len(self.heights)!=self.count**2 or not all(type(h) in (float,int) and math.isfinite(h) and abs(h)<30000 for h in self.heights):
                raise ValueError()
            if not all(math.isfinite(v) and abs(v)<50000 for v in (self.e,self.n)):
                raise ValueError()
        except (KeyError,TypeError,ValueError):
            raise TerrainUnavailable('Terrain data unavailable') from None

    def sample(self,e,n):
        x=(e-self.e)/self.step;y=(n-self.n)/self.step
        if not (0<=x<self.count-1 and 0<=y<self.count-1):
            raise TerrainUnavailable('Loading terrain ahead')
        i=int(x);j=int(y);u=x-i;v=y-j;k=j*self.count+i
        a,b,c,d=(self.heights[k],self.heights[k+1],self.heights[k+self.count],self.heights[k+self.count+1])
        height=(1-v)*((1-u)*a+u*b)+v*((1-u)*c+u*d)
        # Maximum edge grade, not a central derivative which could hide a ridge.
        slope=max(abs(b-a),abs(c-a),abs(d-b),abs(d-c))/self.step
        return height,slope,max(a,b,c,d)


class FlightTerrain:
    def __init__(self,world):
        self.world=world
        self.magnification=world.get('movement_magnification',1.)
        number(self.magnification,1,10,'magnification')
        self.patch=None;self.ready=False;self.origin=None
        self.message='Loading terrain';self.blocked=False;self.started=False
        self.takeoff_roll=False
        from shapely.geometry import shape
        self.water=shape(world['water_geojson'])

    def receive(self,packet):
        data=packet.get('terrain',{})
        if not isinstance(data,dict): data={}
        self.ready=data.get('ready') is True
        if not self.ready:
            self.message=data.get('message','Loading terrain')
            return
        try:
            llh=data['origin']
            if len(llh)!=3 or not all(type(v) in (float,int) and math.isfinite(v) for v in llh): raise ValueError()
            if not (-180<=llh[0]<=180 and -85<=llh[1]<=85 and -1000<=llh[2]<=10000): raise ValueError()
            if self.origin is not None and llh!=self.origin: raise ValueError('Origin changed during session')
            if self.origin is None:
                from pyproj import Transformer
                self.origin=list(llh)
                self.to_ecef=Transformer.from_crs(4979,4978,always_xy=True)
                self.from_ecef=Transformer.from_crs(4978,4979,always_xy=True)
                self.xyz=self.to_ecef.transform(*llh)
                lon,lat=map(math.radians,llh[:2])
                self.east=(-math.sin(lon),math.cos(lon),0.)
                self.north=(-math.sin(lat)*math.cos(lon),-math.sin(lat)*math.sin(lon),math.cos(lat))
            self.patch=HeightPatch(data['patch'])
            if not self.blocked: self.message=''
        except (KeyError,ValueError,TypeError):
            self.ready=False;self.message='Terrain data unavailable'

    def lon_lat(self,e,n):
        xyz=[self.xyz[i]+e*self.east[i]+n*self.north[i] for i in range(3)]
        return self.from_ecef.transform(*xyz)[:2]

    def on_water(self,e,n):
        from shapely.geometry import Point
        return self.water.covers(Point(*self.lon_lat(e,n)))

    def initialize_ride(self,s):
        if not self.ready or self.origin is None: return
        ground,_,_=self.patch.sample(s.position_x_meters,s.position_y_meters)
        s.terrain_enabled=True;s.ground_height_meters=ground
        s.movement_magnification=self.magnification
        s.flight.altitude_meters=ground
        if self.world['start_mode']=='air':
            s.flight_enabled=True;s.flight.airborne=True
            s.flight.altitude_meters=ground+self.world['start_agl_m']
            s.flight.airspeed_meters_per_second=24/3.6
            s.flight.pitch_degrees=3
            s.digital_controls.pitch_degrees=3
        s.reset_recovery_trail()
        self.started=True;self.blocked=False
        self.takeoff_roll=False
        self.telemetry(s)

    def recover(self,s):
        if not self.ready: return False
        target=max(0,s.recovery_path_distance_meters-2)
        for pose in reversed(s.recovery_trail):
            if pose[0]>target: continue
            try: ground,slope,_=self.patch.sample(pose[1],pose[2])
            except TerrainUnavailable: continue
            flight=pose[4]
            if flight.airborne:
                if flight.altitude_meters<ground+.3: continue
            elif slope>.12 or self.on_water(pose[1],pose[2]): continue
            s.last_recovered_meters=max(0,s.recovery_path_distance_meters-pose[0])
            s.position_x_meters,s.position_y_meters,s.heading_degrees=pose[1:4]
            s.flight=replace(flight)
            s.flight_enabled=pose[5]
            s.ground_height_meters=ground
            if not s.flight.airborne: s.flight.altitude_meters=ground
            self.blocked=False;self.message='Recovered 2 m'
            self.takeoff_roll=flight.airborne and flight.altitude_meters-ground<.5
            s.reset_recovery_trail();self.telemetry(s)
            return True
        self.message='No verified recovery point; Esc to restart'
        return False

    def advance(self,s,delta,now):
        s.world_velocity_mps=(0.,0.,0.)
        s.world_speed_kmh=0.;s.world_vertical_speed_mps=0.
        if not self.ready or self.blocked: return 0.
        before=(s.position_x_meters,s.position_y_meters,s.heading_degrees,replace(s.flight),s.distance_meters,s.speed_kmh,s.laps_completed,list(s.recovery_trail),s.recovery_path_distance_meters,s.last_recovery_sample_meters)
        def restore():
            (s.position_x_meters,s.position_y_meters,s.heading_degrees,s.flight,s.distance_meters,s.speed_kmh,s.laps_completed,s.recovery_trail,s.recovery_path_distance_meters,s.last_recovery_sample_meters)=before
        try:
            s.ground_height_meters=self.patch.sample(s.position_x_meters,s.position_y_meters)[0]
            moved=s.advance_flight(delta,now) if s.flight_enabled else s.advance_ground(delta)
            # Scale displacement, never time, aerodynamic speed or attitude.
            # Absolute altitude and initial AGL must not be multiplied.
            s.position_x_meters=before[0]+(s.position_x_meters-before[0])*self.magnification
            s.position_y_meters=before[1]+(s.position_y_meters-before[1])*self.magnification
            if before[3].airborne:
                s.flight.altitude_meters=before[3].altitude_meters+(s.flight.altitude_meters-before[3].altitude_meters)*self.magnification
            if not before[3].airborne and s.flight.airborne:
                self.takeoff_roll=True
            e,n=s.position_x_meters,s.position_y_meters
            if math.hypot(e,n)>self.world['navigation_radius_m']:
                raise RuntimeError('Flight boundary; Button 1 to recover')
            samples=max(1,math.ceil(math.sqrt((e-before[0])**2+(n-before[1])**2+(s.flight.altitude_meters-before[3].altitude_meters)**2)))
            landed=False
            for index in range(samples+1):
                t=index/samples;x=before[0]+t*(e-before[0]);y=before[1]+t*(n-before[1])
                ground,slope,_=self.patch.sample(x,y)
                height=before[3].altitude_meters+t*(s.flight.altitude_meters-before[3].altitude_meters)
                if not before[3].airborne:
                    if slope>.12 or self.on_water(x,y): raise RuntimeError('Ground boundary; Button 1 to recover')
                    s.flight.altitude_meters=ground
                elif height<=ground+.05:
                    # During takeoff the wheels can remain on a gently rising
                    # runway while lift builds. Keep vertical momentum until
                    # clearance is established, instead of treating the first
                    # millimetres of rising ground as an airborne collision.
                    if self.takeoff_roll and s.flight.vertical_speed_meters_per_second>=0 and slope<=.12 and not self.on_water(x,y):
                        s.flight.altitude_meters=max(s.flight.altitude_meters,ground)
                        continue
                    descent=s.flight.vertical_speed_meters_per_second*self.magnification
                    gentle=(descent<=0 and descent>=-2.5 and slope<=.12 and abs(s.flight.bank_degrees)<12 and not self.on_water(x,y))
                    if not gentle: raise RuntimeError('Terrain contact; Button 1 to recover')
                    # Land at first contact, not beyond the intersection.
                    s.position_x_meters=x;s.position_y_meters=y
                    s.flight.altitude_meters=ground
                    s.flight.vertical_speed_meters_per_second=0
                    s.flight.flight_path_angle_degrees=0
                    s.flight.pitch_degrees=0;s.flight.bank_degrees=0
                    s.flight.heading_rate_degrees_per_second=0
                    s.flight.airborne=False;s.flight.stalled=False
                    s.last_flight_event='LANDED';landed=True;break
            self.message='';self.telemetry(s)
            if landed or s.altitude_agl_m>.5: self.takeoff_roll=False
            # Replace the runtime's provisional history entry with validated height.
            s.recovery_trail=before[7];s.recovery_path_distance_meters=before[8];s.last_recovery_sample_meters=before[9]
            actual=math.hypot(s.position_x_meters-before[0],s.position_y_meters-before[1])
            s.distance_meters=before[4]+actual
            s.laps_completed=int(s.distance_meters/max(1.,c.DEFAULT_LAP_LENGTH_METERS))
            if delta>0:
                s.world_velocity_mps=((s.position_x_meters-before[0])/delta,(s.position_y_meters-before[1])/delta,(s.flight.altitude_meters-before[3].altitude_meters)/delta)
                s.world_speed_kmh=actual/delta*3.6
                s.world_vertical_speed_mps=s.world_velocity_mps[2]
            s.record_recovery_pose(actual)
            return actual
        except TerrainUnavailable as exc:
            restore();self.message=str(exc);return 0.
        except RuntimeError as exc:
            restore();self.blocked=True;self.message=str(exc);return 0.

    def telemetry(self,s):
        if self.origin is None or self.patch is None: return
        try:
            ground,_,_=self.patch.sample(s.position_x_meters,s.position_y_meters)
            s.ground_height_meters=ground
            s.longitude,s.latitude=self.lon_lat(s.position_x_meters,s.position_y_meters)
            s.altitude_ellipsoid_m=self.origin[2]+s.flight.altitude_meters
            s.altitude_msl_m=s.altitude_ellipsoid_m-geoid_offset(s.longitude,s.latitude)
            s.altitude_agl_m=s.flight.altitude_meters-ground
        except TerrainUnavailable:
            pass
