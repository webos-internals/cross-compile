diff --git a/navit/navit/osd/core/osd_core.c b/navit/navit/osd/core/osd_core.c
===================================================================
--- a/navit/navit/osd/core/osd_core.c
+++ b/navit/navit/osd/core/osd_core.c
@@ -49,12 +49,21 @@
 #include "osd.h"
 #include "speech.h"
 #include "event.h"
+#include "mapset.h"
 
+//prototypes
 struct odometer;
 
 static void osd_odometer_reset(struct odometer *this);
 static void osd_cmd_odometer_reset(struct navit *this, char *function, struct attr **in, struct attr ***out, int *valid);
 static void osd_odometer_draw(struct odometer *this, struct navit *nav, struct vehicle *v);
+struct quadtree_node* quadtree_node_new(struct quadtree_node* parent, double xmin, double xmax, double ymin, double ymax );
+struct quadtree_item* quadtree_find_nearest_flood(struct quadtree_node* this_, struct quadtree_item* item, double current_max, struct quadtree_node* toSkip);
+struct quadtree_item* quadtree_find_nearest(struct quadtree_node* this_, struct quadtree_item* item);
+void quadtree_split(struct quadtree_node* this_);
+void quadtree_add(struct quadtree_node* this_, struct quadtree_item* item);
+void quadtree_destroy(struct quadtree_node* this_);
+struct quadtree_node* osd_speed_cam_load(char* fn);
 
 struct compass {
 	struct osd_item osd_item;
@@ -1239,6 +1248,686 @@
 	return (struct osd_priv *) this;
 }
 
+enum camera_t {CAM_FIXED=1, CAM_TRAFFIC_LAMP, CAM_RED, CAM_SECTION, CAM_MOBILE, CAM_RAIL, CAM_TRAFFIPAX};
+char*camera_t_strs[] = {"None","Fix","Traffic lamp","Red detect","Section","Mobile","Rail","Traffipax(non persistent)"};
+char*camdir_t_strs[] = {"All dir.","UNI-dir","BI-dir"};
+enum cam_dir_t {CAMDIR_ALL=0, CAMDIR_ONE, CAMDIR_TWO};
+
+struct quadtree_item {
+    double longitude;
+    double latitude;
+    void*  data;
+};
+
+#define QUADTREE_NODE_CAPACITY 10
+
+struct quadtree_node {
+    int node_num;
+    struct quadtree_item items[QUADTREE_NODE_CAPACITY];
+    struct quadtree_node *aa;
+    struct quadtree_node *ab;
+    struct quadtree_node *ba;
+    struct quadtree_node *bb;
+    double xmin, xmax, ymin, ymax;
+    int is_leaf;
+    struct quadtree_node*parent;
+};
+
+struct quadtree_node*
+quadtree_node_new(struct quadtree_node* parent, double xmin, double xmax, double ymin, double ymax ) {
+    struct quadtree_node*ret = calloc(1, sizeof(struct quadtree_node));
+    ret->xmin = xmin;
+    ret->xmax = xmax;
+    ret->ymin = ymin;
+    ret->ymax = ymax;
+    ret->is_leaf = 1;
+    ret->parent = parent;
+    return ret;
+}
+
+#define MAX_DOUBLE 9999999
+
+static double 
+dist_sq(double x1,double y1,double x2,double y2)
+{
+  return (x2-x1)*(x2-x1)+(y2-y1)*(y2-y1);
+}
+
+/*
+ * searches all four subnodes recursively for the closest item
+ */
+struct quadtree_item*
+quadtree_find_nearest_flood(struct quadtree_node* this_, struct quadtree_item* item, double current_max, struct quadtree_node* toSkip) {
+    struct quadtree_node* nodes[4] = { this_->aa, this_->ab, this_->ba, this_->bb };
+    struct quadtree_item*res = NULL;
+    if( this_->is_leaf ) { 
+        int i;
+        double distance_sq = current_max;
+        for(i=0;i<this_->node_num;++i) {
+            double curr_dist_sq = dist_sq(item->longitude,item->latitude,this_->items[i].longitude,this_->items[i].latitude);
+            if(curr_dist_sq<distance_sq) {
+                distance_sq = curr_dist_sq;
+                res = &this_->items[i];
+            }
+        }
+    }
+    else {
+      int i;
+      for( i=0;i<4;++i) {
+        if(nodes[i] && nodes[i]!=toSkip) {
+          //limit flooding
+          struct quadtree_item*res_tmp = NULL;
+	  if(
+             dist_sq(nodes[i]->xmin,nodes[i]->ymin,item->longitude,item->latitude)<current_max || 
+	     dist_sq(nodes[i]->xmax,nodes[i]->ymin,item->longitude,item->latitude)<current_max ||
+	     dist_sq(nodes[i]->xmax,nodes[i]->ymax,item->longitude,item->latitude)<current_max ||
+	     dist_sq(nodes[i]->xmin,nodes[i]->ymax,item->longitude,item->latitude)<current_max 
+            ) {
+            res_tmp = quadtree_find_nearest_flood(nodes[i],item,current_max,NULL);
+	  }
+	  if(res_tmp) {
+	    res = res_tmp;
+            double curr_dist_sq = dist_sq(item->longitude,item->latitude,res->longitude,res->latitude);
+            if(curr_dist_sq<current_max) {
+                current_max = curr_dist_sq;
+            }
+ 	  }
+	} 
+      }
+    }
+  return res;
+}
+
+/*
+ * tries to find closest item, first it descend into the quadtree as much as possible, then if no point is found
+ */
+struct quadtree_item*
+quadtree_find_nearest(struct quadtree_node* this_, struct quadtree_item* item) {
+    if( ! this_ ) {
+      return NULL;
+    }
+    struct quadtree_item*res = NULL;
+    double distance_sq = MAX_DOUBLE;
+    if( this_->is_leaf ) { 
+        int i;
+        for(i=0;i<this_->node_num;++i) {
+            double curr_dist_sq = dist_sq(item->longitude,item->latitude,this_->items[i].longitude,this_->items[i].latitude);
+            if(curr_dist_sq<distance_sq) {
+                distance_sq = curr_dist_sq;
+                res = &this_->items[i];
+            }
+        }
+      //go up n levels
+	  if(!res && this_->parent) {
+	          struct quadtree_node* anchestor = this_->parent;
+                  int cnt = 0;
+        	  while (anchestor->parent && cnt<4) {
+	            anchestor = anchestor->parent;
+                    ++cnt;
+		  }
+		struct quadtree_item*res2 = NULL;
+	  	res2 = quadtree_find_nearest_flood(anchestor,item,distance_sq,NULL);
+		if(res2) {
+		  res = res2;
+		}
+	  }
+    }
+    else {
+        if(
+	   this_->aa && 
+           this_->aa->xmin<=item->longitude && item->longitude<this_->aa->xmax &&
+           this_->aa->ymin<=item->latitude && item->latitude<this_->aa->ymax
+           ) {
+          res = quadtree_find_nearest(this_->aa,item);
+        }
+        else if(
+	   this_->ab && 
+           this_->ab->xmin<=item->longitude && item->longitude<this_->ab->xmax &&
+           this_->ab->ymin<=item->latitude && item->latitude<this_->ab->ymax
+           ) {
+          res = quadtree_find_nearest(this_->ab,item);
+        }
+        else if(
+	   this_->ba && 
+           this_->ba->xmin<=item->longitude && item->longitude<this_->ba->xmax &&
+           this_->ba->ymin<=item->latitude && item->latitude<this_->ba->ymax
+           ) {
+          res = quadtree_find_nearest(this_->ba,item);
+        }
+        else if(
+	   this_->bb && 
+           this_->bb->xmin<=item->longitude && item->longitude<this_->bb->xmax &&
+           this_->bb->ymin<=item->latitude && item->latitude<this_->bb->ymax
+           ) {
+          res = quadtree_find_nearest(this_->bb,item);
+        }
+	else {
+        if(this_->parent) {
+	    //go up two levels
+	    struct quadtree_node* anchestor = this_->parent;
+            int cnt = 0;
+            while (anchestor->parent && cnt<4) {
+	      anchestor = anchestor->parent;
+              ++cnt;
+	    }
+        res = quadtree_find_nearest_flood(anchestor,item,distance_sq,NULL);
+        }
+      }
+    }
+  return res;
+}
+
+void
+quadtree_add(struct quadtree_node* this_, struct quadtree_item* item) {
+    if( this_->is_leaf ) {
+        this_->items[this_->node_num++] = *item;
+        if(QUADTREE_NODE_CAPACITY == this_->node_num) {
+            quadtree_split(this_);
+        }
+    }
+    else {
+        if(
+           this_->xmin<=item->longitude && item->longitude<this_->xmin+(this_->xmax-this_->xmin)/2.0 &&
+           this_->ymin<=item->latitude && item->latitude<this_->ymin+(this_->ymax-this_->ymin)/2.0
+           ) {
+	    if(!this_->aa) {
+              this_->aa = quadtree_node_new( this_, this_->xmin, this_->xmin+(this_->xmax-this_->xmin)/2.0 , this_->ymin, this_->ymin+(this_->ymax-this_->ymin)/2.0 );
+	    }
+          quadtree_add(this_->aa,item);
+        }
+        else if(
+           this_->xmin+(this_->xmax-this_->xmin)/2.0<=item->longitude && item->longitude<this_->xmax &&
+           this_->ymin<=item->latitude && item->latitude<this_->ymin+(this_->ymax-this_->ymin)/2.0
+           ) {
+	    if(!this_->ab) {
+              this_->ab = quadtree_node_new( this_, this_->xmin+(this_->xmax-this_->xmin)/2.0, this_->xmax , this_->ymin, this_->ymin+(this_->ymax-this_->ymin)/2.0 );
+	    }
+          quadtree_add(this_->ab,item);
+        }
+        else if(
+           this_->xmin<=item->longitude && item->longitude<this_->xmin+(this_->xmax-this_->xmin)/2.0 &&
+           this_->ymin+(this_->ymax-this_->ymin)/2.0<=item->latitude && item->latitude<this_->ymax
+           ) {
+	    if(!this_->ba) {
+              this_->ba = quadtree_node_new( this_, this_->xmin, this_->xmin+(this_->xmax-this_->xmin)/2.0 , this_->ymin+(this_->ymax-this_->ymin)/2.0 , this_->ymax);
+	    }
+          quadtree_add(this_->ba,item);
+        }
+        else if(
+           this_->xmin+(this_->xmax-this_->xmin)/2.0<=item->longitude && item->longitude<this_->xmax &&
+           this_->ymin+(this_->ymax-this_->ymin)/2.0<=item->latitude && item->latitude<this_->ymax
+           ) {
+	    if(!this_->bb) {
+              this_->bb = quadtree_node_new( this_, this_->xmin+(this_->xmax-this_->xmin)/2.0, this_->xmax , this_->ymin+(this_->ymax-this_->ymin)/2.0 , this_->ymax);
+	    }
+          quadtree_add(this_->bb,item);
+        }
+    }
+}
+
+void
+quadtree_split(struct quadtree_node* this_) {
+    int i;
+    this_->is_leaf = 0;
+    for(i=0;i<this_->node_num;++i) {
+        if(
+           this_->xmin<=this_->items[i].longitude && this_->items[i].longitude<this_->xmin+(this_->xmax-this_->xmin)/2.0 &&
+           this_->ymin<=this_->items[i].latitude && this_->items[i].latitude<this_->ymin+(this_->ymax-this_->ymin)/2.0
+           ) {
+	  if(!this_->aa) {
+            this_->aa = quadtree_node_new( this_, this_->xmin, this_->xmin+(this_->xmax-this_->xmin)/2.0 , this_->ymin, this_->ymin+(this_->ymax-this_->ymin)/2.0 );
+	  }
+          quadtree_add(this_->aa,&this_->items[i]);
+        }
+        else if(
+           this_->xmin+(this_->xmax-this_->xmin)/2.0<=this_->items[i].longitude && this_->items[i].longitude<this_->xmax &&
+           this_->ymin<=this_->items[i].latitude && this_->items[i].latitude<this_->ymin+(this_->ymax-this_->ymin)/2.0
+           ) {
+	  if(!this_->ab) {
+            this_->ab = quadtree_node_new( this_, this_->xmin+(this_->xmax-this_->xmin)/2.0, this_->xmax , this_->ymin, this_->ymin+(this_->ymax-this_->ymin)/2.0 );
+	  }
+          quadtree_add(this_->ab,&this_->items[i]);
+        }
+        else if(
+           this_->xmin<=this_->items[i].longitude && this_->items[i].longitude<this_->xmin+(this_->xmax-this_->xmin)/2.0 &&
+           this_->ymin+(this_->ymax-this_->ymin)/2.0<=this_->items[i].latitude && this_->items[i].latitude<this_->ymax
+           ) {
+	  if(!this_->ba) {
+            this_->ba = quadtree_node_new( this_, this_->xmin, this_->xmin+(this_->xmax-this_->xmin)/2.0 , this_->ymin+(this_->ymax-this_->ymin)/2.0 , this_->ymax);
+	  }
+          quadtree_add(this_->ba,&this_->items[i]);
+        }
+        else if(
+           this_->xmin+(this_->xmax-this_->xmin)/2.0<=this_->items[i].longitude && this_->items[i].longitude<this_->xmax &&
+           this_->ymin+(this_->ymax-this_->ymin)/2.0<=this_->items[i].latitude && this_->items[i].latitude<this_->ymax
+           ) {
+	  if(!this_->bb) {
+            this_->bb = quadtree_node_new( this_, this_->xmin+(this_->xmax-this_->xmin)/2.0, this_->xmax , this_->ymin+(this_->ymax-this_->ymin)/2.0 , this_->ymax);
+	  }
+          quadtree_add(this_->bb,&this_->items[i]);
+        }
+    }
+    this_->node_num = 0;
+}
+
+void
+quadtree_destroy(struct quadtree_node* this_) {
+    if(this_->aa) {
+        quadtree_destroy(this_->aa);
+    }
+    if(this_->ab) {
+        quadtree_destroy(this_->ab);
+    }
+    if(this_->ba) {
+        quadtree_destroy(this_->ba);
+    }
+    if(this_->bb) {
+        quadtree_destroy(this_->bb);
+    }
+    free(this_);
+}
+
+
+
+struct osd_speed_cam_entry {
+	double lon;
+	double lat;
+	enum camera_t cam_type;
+	int speed_limit;
+	enum cam_dir_t cam_dir;
+	int direction;
+};
+
+enum eAnnounceState {eNoWarn=0,eWarningTold=1};
+
+struct osd_speed_cam {
+  struct osd_item item;
+  int width;
+  struct graphics_gc *white,*orange;
+  struct graphics_gc *red;
+  struct color idle_color; 
+
+  struct quadtree_node*root_node;
+  int announce_on;
+  enum eAnnounceState announce_state;
+  char *text;                 //text of label attribute for this osd
+};
+
+static double 
+angle_diff(firstAngle,secondAngle)
+{
+        double difference = secondAngle - firstAngle;
+        while (difference < -180) difference += 360;
+        while (difference > 180) difference -= 360;
+        return difference;
+}
+
+static void
+osd_speed_cam_draw(struct osd_speed_cam *this_, struct navit *navit, struct vehicle *v)
+{
+  struct attr position_attr,vehicle_attr;
+  struct point p, bbox[4];
+  struct attr speed_attr;
+  struct vehicle* curr_vehicle = v;
+  struct coord curr_coord;
+  struct coord cam_coord;
+  if(navit) {
+    navit_get_attr(navit, attr_vehicle, &vehicle_attr, NULL);
+  }
+  else {
+    return;
+  }
+  if (vehicle_attr.u.vehicle) {
+    curr_vehicle = vehicle_attr.u.vehicle;
+  }
+
+  if(0==curr_vehicle)
+    return;
+
+  int ret_attr = 0;
+  osd_std_draw(&this_->item);
+
+  ret_attr = vehicle_get_attr(curr_vehicle, attr_position_coord_geo,&position_attr, NULL);
+  if(0==ret_attr) {
+    return;
+  }
+  transform_from_geo(projection_mg, position_attr.u.coord_geo, &curr_coord);
+
+  double dCurrDist = -1;
+  int dir_idx = -1;
+  int dir = -1;
+  int spd = -1;
+  int idx = -1;
+  double speed = -1;
+
+  if (this_->root_node) {
+    struct quadtree_item qi;
+    qi.longitude = position_attr.u.coord_geo->lng;
+    qi.latitude = position_attr.u.coord_geo->lat;
+    struct quadtree_item*qi_res = quadtree_find_nearest(this_->root_node,&qi);
+    struct coord_geo coord_geo_cam;
+    if(qi_res) {  //found nearest camera
+      coord_geo_cam.lng = qi_res->longitude;
+      coord_geo_cam.lat = qi_res->latitude;
+      transform_from_geo(projection_mg, &coord_geo_cam, &cam_coord);
+      dCurrDist = transform_distance(projection_mg, &curr_coord, &cam_coord);
+      idx = ((struct osd_speed_cam_entry*)qi_res->data)->cam_type;
+      if(idx>CAM_TRAFFIPAX) {
+        return;
+      }
+      dir_idx = ((struct osd_speed_cam_entry*)qi_res->data)->cam_dir;
+      dir = ((struct osd_speed_cam_entry*)qi_res->data)->direction;
+      spd = ((struct osd_speed_cam_entry*)qi_res->data)->speed_limit;
+    }
+  }
+  else if(navit_get_mapset(navit)) {
+
+  int dst=2000;
+  int dstsq=dst*dst;
+  struct map_selection sel;
+  struct map_rect *mr;
+  struct mapset_handle *msh;
+  struct map *map;
+  struct item *item;
+
+  sel.next=NULL;
+  sel.order=18;
+  sel.range.min=type_tec_common;
+  sel.range.max=type_tec_common;
+  sel.u.c_rect.lu.x=curr_coord.x-dst;
+  sel.u.c_rect.lu.y=curr_coord.y+dst;
+  sel.u.c_rect.rl.x=curr_coord.x+dst;
+  sel.u.c_rect.rl.y=curr_coord.y-dst;
+  
+  msh=mapset_open(navit_get_mapset(navit));
+  while ((map=mapset_next(msh, 1))) {
+    mr=map_rect_new(map, &sel);
+    if (!mr)
+      continue;
+    while ((item=map_rect_get_item(mr))) {
+      struct coord cn;
+      if (item->type == type_tec_common && item_coord_get(item, &cn, 1)) {
+        int dist=transform_distance_sq(&cn, &curr_coord);
+        if (dist < dstsq) {  
+          dstsq=dist;
+          dCurrDist = sqrt(dist);
+          cam_coord = cn;
+          struct attr tec_attr;
+          idx = -1;
+          if(item_attr_get(item,attr_tec_type,&tec_attr)) {
+            idx = tec_attr.u.num;
+          }
+          dir_idx = -1;
+          if(item_attr_get(item,attr_tec_dirtype,&tec_attr)) {
+            dir_idx = tec_attr.u.num;
+          }
+          dir= 0;
+          if(item_attr_get(item,attr_tec_direction,&tec_attr)) {
+            dir = tec_attr.u.num;
+          }
+          spd= 0;
+          if(item_attr_get(item,attr_maxspeed,&tec_attr)) {
+            spd = tec_attr.u.num;
+          }
+        }
+      }
+    }
+    map_rect_destroy(mr);
+  }
+  mapset_close(msh);
+
+    dCurrDist = transform_distance(projection_mg, &curr_coord, &cam_coord);
+
+  }
+
+  dCurrDist = transform_distance(projection_mg, &curr_coord, &cam_coord);
+  ret_attr = vehicle_get_attr(curr_vehicle,attr_position_speed,&speed_attr, NULL);
+  if(0==ret_attr) {
+    return;
+  }
+  speed = *speed_attr.u.numd;
+  if(dCurrDist <= speed*750.0/130.0) {  //at speed 130 distance limit is 750m
+    if(this_->announce_state==eNoWarn && this_->announce_on) {
+      this_->announce_state=eWarningTold; //warning told
+      navit_say(navit, _("Look out! Camera!"));
+    }
+  }
+  else {
+    this_->announce_state=eNoWarn;
+  }
+
+  char buffer [64+1]="";
+  char buffer2[64+1]="";
+  buffer [0] = 0;
+  buffer2[0] = 0;
+  if(this_->text) {
+    str_replace(buffer,this_->text,"${distance}",format_distance(dCurrDist,""));
+    str_replace(buffer2,buffer,"${camera_type}",camera_t_strs[idx]);
+    str_replace(buffer,buffer2,"${camera_dir}",camdir_t_strs[dir_idx]);
+    char dir_str[16];
+    char spd_str[16];
+    sprintf(dir_str,"%d",dir);
+    sprintf(spd_str,"%d",spd);
+    str_replace(buffer2,buffer,"${direction}",dir_str);
+    str_replace(buffer,buffer2,"${speed_limit}",spd_str);
+
+  graphics_get_text_bbox(this_->item.gr, this_->item.font, buffer, 0x10000, 0, bbox, 0);
+  p.x=(this_->item.w-bbox[2].x)/2;
+  p.y = this_->item.h-this_->item.h/10;
+  struct graphics_gc *curr_color = this_->orange;
+  struct attr attr_dir;
+  //tolerance is +-20 degrees
+  if(
+    dir_idx==CAMDIR_ONE && 
+    dCurrDist <= speed*750.0/130.0 && 
+    vehicle_get_attr(v, attr_position_direction, &attr_dir, NULL) && 
+    fabs(angle_diff(dir,*attr_dir.u.numd))<=20 ) {
+      curr_color = this_->red;
+  }
+  //tolerance is +-20 degrees in both directions
+  else if(
+    dir_idx==CAMDIR_TWO && 
+    dCurrDist <= speed*750.0/130.0 && 
+    vehicle_get_attr(v, attr_position_direction, &attr_dir, NULL) && 
+    (fabs(angle_diff(dir,*attr_dir.u.numd))<=20 || fabs(angle_diff(dir+180,*attr_dir.u.numd))<=20 )) {
+      curr_color = this_->red;
+  }
+        else if(dCurrDist <= speed*750.0/130.0) { 
+      curr_color = this_->red;
+        }
+  graphics_draw_text(this_->item.gr, curr_color, NULL, this_->item.font, buffer, &p, 0x10000, 0);
+    }
+  graphics_draw_mode(this_->item.gr, draw_mode_end);
+}
+
+static void
+osd_speed_cam_init(struct osd_speed_cam *this, struct navit *nav)
+{
+  osd_set_std_graphic(nav, &this->item, (struct osd_priv *)this);
+
+  this->red = graphics_gc_new(this->item.gr);
+  graphics_gc_set_foreground(this->red, &(struct color) {0xffff,0x0000,0x0000,0xffff});
+  graphics_gc_set_linewidth(this->red, this->width);
+
+  this->orange = graphics_gc_new(this->item.gr);
+  graphics_gc_set_foreground(this->orange, &this->idle_color);
+  graphics_gc_set_linewidth(this->orange, this->width);
+
+  this->white = graphics_gc_new(this->item.gr);
+  graphics_gc_set_foreground(this->white, &this->item.text_color);
+  graphics_gc_set_linewidth(this->white, this->width);
+
+
+  graphics_gc_set_linewidth(this->item.graphic_fg_white, this->width);
+
+  navit_add_callback(nav, callback_new_attr_1(callback_cast(osd_speed_cam_draw), attr_position_coord_geo, this));
+
+}
+
+struct quadtree_node*
+osd_speed_cam_load(char* fn)
+{
+    FILE*fp;
+    if((fp=fopen(fn,"rt"))) {
+        char  line[128];
+        char* line2;
+        char* lat_str;
+        char* lon_str;
+        char* type_str;
+        char* spd_str;
+        char* dirtype_str;
+        char* direction_str;
+        struct quadtree_node*root_node = quadtree_node_new(NULL,-90,90,-90,90);
+        /*skip header lines*/
+        fgets(line,127,fp);
+
+        while(fgets(line,127,fp)) {
+          if(!strcmp(line,"")) {
+          continue;
+        }
+            line2 = g_strdup(line);
+            /*count tokens*/
+            int tok_cnt = 1;
+            if( strtok(line2,",")) {
+              while( strtok(NULL,",")) {
+                ++tok_cnt;
+              }
+            }
+            g_free(line2);
+            if (7==tok_cnt) {
+              strtok(line,","); //skip idx
+              if(lon_str) {
+              }
+              else {
+                  continue;
+              }
+              lon_str = strtok(NULL,",");
+              if(lon_str) {
+              }
+              else {
+                  continue;
+              }
+            }
+            else if (6==tok_cnt) {
+              lon_str = strtok(line,",");
+              if(lon_str) {
+              }
+              else {
+                  continue;
+              }
+            }
+            else { //illegal format
+              continue;
+            }
+
+            lat_str = strtok(NULL,",");
+            if(lat_str) {
+            }
+            else {
+                continue;
+            }
+
+            type_str = strtok(NULL,",");
+            if(type_str) {
+            }
+            else {
+                continue;
+            }
+
+            spd_str = strtok(NULL,",");
+            if(spd_str) {
+            }
+            else {
+                continue;
+            }
+            dirtype_str = strtok(NULL,",");
+            if(dirtype_str) {
+            }
+            else {
+                continue;
+            }
+            direction_str = strtok(NULL,",");
+            if(direction_str) {
+            }
+            else {
+                continue;
+            }
+
+            struct quadtree_item item;
+            item.longitude = atof(lon_str);
+            item.latitude  = atof(lat_str);
+            if(item.longitude==0 && item.latitude==0) {
+              continue;
+            }
+            struct osd_speed_cam_entry*sc_entry = malloc(sizeof(struct osd_speed_cam_entry));
+            sc_entry->lon         = item.longitude;
+            sc_entry->lat         = item.latitude;
+            sc_entry->cam_type    = atoi(type_str);
+            sc_entry->speed_limit = atoi(spd_str);
+            sc_entry->cam_dir     = atoi(dirtype_str);
+            sc_entry->direction   = atoi(direction_str);
+            item.data = sc_entry;
+            if(sc_entry->lon != 0) {
+              quadtree_add(root_node,&item);
+            }
+        }
+        return root_node;
+    }
+    else {
+        return NULL;
+    }
+  return NULL;
+}
+
+
+static struct osd_priv *
+osd_speed_cam_new(struct navit *nav, struct osd_methods *meth, struct attr **attrs)
+{
+  struct osd_speed_cam *this = g_new0(struct osd_speed_cam, 1);
+  struct attr *attr;
+  this->item.p.x = 120;
+  this->item.p.y = 20;
+  this->item.w = 60;
+  this->item.h = 80;
+  this->item.navit = nav;
+  this->item.font_size = 200;
+  this->item.meth.draw = osd_draw_cast(osd_speed_cam_draw);
+
+  osd_set_std_attr(attrs, &this->item, 2);
+  attr = attr_search(attrs, NULL, attr_width);
+  this->width=attr ? attr->u.num : 2;
+  attr = attr_search(attrs, NULL, attr_idle_color);
+  this->idle_color=attr ? *attr->u.color : ((struct color) {0xffff,0xa5a5,0x0000,0xffff}); // text idle_color defaults to orange
+
+  attr = attr_search(attrs, NULL, attr_label);
+  //FIXME find some way to free text!!!!
+  if (attr) {
+    this->text = g_strdup(attr->u.str);
+  }
+  else
+    this->text = NULL;
+
+  attr = attr_search(attrs, NULL, attr_path);
+  char*fn;
+  if (attr) {
+    fn = attr->u.str;
+  }
+  else {
+    fn = g_strdup_printf("%s/speedcam.txt",navit_get_user_data_directory(TRUE));
+  }
+  this->root_node = osd_speed_cam_load(fn);
+
+  attr = attr_search(attrs, NULL, attr_announce_on);
+  if (attr) {
+    this->announce_on = attr->u.num;
+  }
+  else {
+    this->announce_on = 1;    //announce by default
+  }
+
+  navit_add_callback(nav, callback_new_attr_1(callback_cast(osd_speed_cam_init), attr_graphics_ready, this));
+  return (struct osd_priv *) this;
+}
 struct osd_speed_warner {
 	struct osd_item item;
 	struct graphics_gc *red;
@@ -1252,7 +1941,6 @@
 	double speed_exceed_limit_offset;
 	double speed_exceed_limit_percent;
 	int announce_on;
-        enum eAnnounceState {eNoWarn=0,eWarningTold=1};
 	enum eAnnounceState announce_state;
 	int bTextOnly;
 };
@@ -2484,6 +3172,7 @@
 	plugin_register_osd_type("button", osd_button_new);
     	plugin_register_osd_type("toggle_announcer", osd_nav_toggle_announcer_new);
     	plugin_register_osd_type("speed_warner", osd_speed_warner_new);
+    	plugin_register_osd_type("speed_cam", osd_speed_cam_new);
     	plugin_register_osd_type("text", osd_text_new);
     	plugin_register_osd_type("gps_status", osd_gps_status_new);
     	plugin_register_osd_type("volume", osd_volume_new);
