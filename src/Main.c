//#define IMAGE_PNG_ONLY

#include "/home/codeleaded/System/Static/Library/WindowEngine1.0.h"
#include "/home/codeleaded/System/Static/Library/Files.h"
#include "/home/codeleaded/System/Static/Library/TransformedView.h"
#include "/home/codeleaded/System/Static/Library/Geometry.h"

#define BLOCK_NONE		0
#define BLOCK_DIRT		1
#define BLOCK_GRAS		2
#define BLOCK_BRICK		3
#define BLOCK_QM		4
#define BLOCK_COIN		6
#define BLOCK_PODEST	7
#define BLOCK_PILZ		8
#define BLOCK_OPENQM	9

typedef unsigned char Block;

typedef struct World{
	Vector imgs;
	Block* data;
	unsigned short width;
	unsigned short height;
} World;

World World_New(unsigned short width,unsigned short height){
	World w;
	w.imgs = Vector_New(sizeof(Sprite));
	w.data = (Block*)malloc(sizeof(Block) * width * height);
	w.width = width;
	w.height = height;
	return w;
}
Block World_Std_Mapper(char c){
	switch (c){
	case '.': return BLOCK_NONE;
	case 'e': return BLOCK_DIRT;
	case 'g': return BLOCK_GRAS;
	case '#': return BLOCK_BRICK;
	case '?': return BLOCK_QM;
	case 'q': return BLOCK_QM;
	case 'o': return BLOCK_COIN;
	case 'p': return BLOCK_PODEST;
	}
	return BLOCK_NONE;
}
void World_Load(World* w,char* Path,Block (*MapperFunc)(char c)){
	FilesSize size;
	char* data = Files_ReadB(Path,&size);

	int width = CStr_Find(data,'\n');
	int height = CStr_CountOf(data,'\n');

	if(w->data) free(w->data);
	w->data = (Block*)malloc(sizeof(Block) * width * height);
	w->width = width;
	w->height = height;

	for(int i = 0;i<size;i++){
		int y = i / (width + 1);
		int x = i - y * (width + 1);
		int j = y * width + x;

		w->data[j] = MapperFunc(data[i]);
	}
}
World World_Make(char* Path,Block (*MapperFunc)(char c),Sprite* s){
	World w;
	w.imgs = Vector_New(sizeof(Sprite));
	w.data = NULL;
	w.width = 0;
	w.height = 0;

	World_Load(&w,Path,MapperFunc);

	for(int i = 0;s[i].img;i++){
		Vector_Push(&w.imgs,&s[i]);
	}
	return w;
}
Block World_Get(World* w,unsigned int x,unsigned int y){
	if(x<w->width && y<w->height) return w->data[y * w->width + x];
	return BLOCK_NONE;
}
void World_Set(World* w,unsigned int x,unsigned int y,Block b){
	if(x<w->width && y<w->height) w->data[y * w->width + x] = b;
}
Sprite* World_Get_Img(World* w,unsigned int x,unsigned int y){
	const Block b = World_Get(w,x,y);
	if(b!=BLOCK_NONE){
		if(b<w->imgs.size){
			Sprite* s = (Sprite*)Vector_Get(&w->imgs,b);
			return s;
		}
	}
	return NULL;
}
void World_Resize(World* w,unsigned int width,unsigned int height){
	for(int i = 0;i<w->imgs.size;i++){
		Sprite* s = (Sprite*)Vector_Get(&w->imgs,i);
		
		if(s->w != width || s->h != height){
			Sprite_Reload(s,width,height);
		}
	}
}
void World_Render(World* w,TransformedView* tv,Pixel* out,unsigned int width,unsigned int height){
	const Vec2 tl = TransformedView_ScreenWorldPos(tv,(Vec2){ 0.0f,0.0f });
	const Vec2 br = TransformedView_ScreenWorldPos(tv,(Vec2){ width,height });

	const Vec2 sd = TransformedView_WorldScreenLength(tv,(Vec2){ 1.0f,1.0f });

	World_Resize(w,(unsigned int)F32_Ceil(sd.x),(unsigned int)F32_Ceil(sd.y));
	
	for(int y = tl.y;y<br.y;y++){
		for(int x = tl.x;x<br.x;x++){
			const Block b = World_Get(w,x,y);

			if(b!=BLOCK_NONE){
				const Vec2 sc = TransformedView_WorldScreenPos(tv,(Vec2){ x,y });
				Sprite* s = World_Get_Img(w,x,y);
				if(s) RenderSpriteAlpha(s,sc.x,sc.y);
			}
		}
	}
}
void World_Free(World* w){
	for(int i = 0;i<w->imgs.size;i++){
		Sprite* s = (Sprite*)Vector_Get(&w->imgs,i);
		Sprite_Free(s);
	}
	Vector_Free(&w->imgs);
	
	if(w->data) free(w->data);
	w->data = NULL;
}

char World_Block_IsPickUp(World* w,unsigned int x,unsigned int y){
	Block b = World_Get(w,x,y);

	if(b==BLOCK_COIN){
		World_Set(w,x,y,BLOCK_NONE);
		return 1;
	}else if(b==BLOCK_PILZ){
		World_Set(w,x,y,BLOCK_NONE);
		return 1;
	}
	return 0;
}
void World_Block_Collision(World* w,unsigned int x,unsigned int y,Side s){
	Block b = World_Get(w,x,y);

	if(s==SIDE_BOTTOM){
		if(b==BLOCK_BRICK) World_Set(w,x,y,BLOCK_NONE);
		if(b==BLOCK_QM){
			World_Set(w,x,y,BLOCK_OPENQM);
			World_Set(w,x,y-1,BLOCK_PILZ);
		}
	}
}

#define MARIO_GROUND_FALSE	0
#define MARIO_GROUND_TRUE	1

typedef struct Figure{
	Vector imgs;
	Rect r;
	Vec2 v;
	Vec2 a;
	Timepoint start;
	unsigned char ground;
} Figure;

Figure Figure_New(Vec2 p,Vec2 d){
	Figure f;
	f.imgs = Vector_New(sizeof(Sprite));
	f.r = Rect_New(p,d);
	f.v = (Vec2){ 0.0f,0.0f };
	f.a = (Vec2){ 0.0f,20.0f };

	f.start = 0UL;

	f.ground = MARIO_GROUND_FALSE;
	return f;
}
Figure Figure_Make(Vec2 p,Vec2 d,Sprite* s){
	Figure f = Figure_New(p,d);
	
	for(int i = 0;s[i].img && s[i].w>0 && s[i].h>0;i++){
		Vector_Push(&f.imgs,&s[i]);
	}
	return f;
}
void Figure_Update(Figure* f,float t){
	f->v = Vec2_Add(f->v,Vec2_Mulf(f->a,t));
	f->r.p = Vec2_Add(f->r.p,Vec2_Mulf(f->v,t));

	if(f->r.p.x < 0.0f) f->r.p.x = 0.0f;
	if(f->r.p.y < 0.0f) f->r.p.y = 0.0f;
}
void Figure_Collision(Figure* f,World* w,int (*Rect_Rect_Compare)(const void*,const void*)){
	Vector rects = Vector_New(sizeof(Rect));
	
	float searchX = F32_Max(2.0f,2.0f * f->r.d.x);
	float searchY = F32_Max(2.0f,2.0f * f->r.d.y);

	for(float x = -searchX;x<searchX;x+=1.0f) {
		for(float y = -searchY;y<searchY;y+=1.0f) {
			int sx = (int)(f->r.p.x + x);
			int sy = (int)(f->r.p.y + y);
			
            if(sy>=0 && sy<w->height && sx>=0 && sx<w->width) {
                Block b = World_Get(w,sx,sy);
                Rect br = { (Vec2){ sx,sy },(Vec2){ 1.0f,1.0f } };

				if(Overlap_Rect_Rect(f->r,br)){
					if(!World_Block_IsPickUp(w,sx,sy) && b!=BLOCK_NONE){
						Vector_Push(&rects,&br);
					}
				}
			}
		}
	}
	
    f->ground = MARIO_GROUND_FALSE;
	
	qsort(rects.Memory,rects.size,rects.ELEMENT_SIZE,Rect_Rect_Compare);
	
	for(int i = 0;i<rects.size;i++) {
        Rect rect = *(Rect*)Vector_Get(&rects,i);
		
		Side s = Resolve_Rect_Rect(&f->r,rect);

		if(s==SIDE_TOP) 					f->ground = MARIO_GROUND_TRUE;
		if(s==SIDE_TOP && f->v.y>0.0f) 		f->v.y = 0.0f;
		if(s==SIDE_BOTTOM && f->v.y<0.0f) 	f->v.y = 0.0f;

		World_Block_Collision(w,rect.p.x,rect.p.y,s);
	}

	Vector_Free(&rects);
}
Sprite* Figure_Get_Img(Figure* f){
	FDuration d = Time_Elapsed(f->start,Time_Nano());
	d /= F32_Abs(f->v.x) * 0.1f;
	d = d - F32_Floor(d);
	int aimg = (int)(5.0f * d);

	if(f->v.x==0.0f) 	aimg = 5;
	if(!f->ground) 		aimg = 6;
	aimg = 2 * aimg + (f->v.x<0.0f ? 1 : 0);

	if(aimg>=f->imgs.size) return NULL;

	Sprite* s = (Sprite*)Vector_Get(&f->imgs,aimg);
	return s;
}
void Figure_Resize(Figure* f,unsigned int w,unsigned int h){
	for(int i = 0;i<f->imgs.size;i++){
		Sprite* s = (Sprite*)Vector_Get(&f->imgs,i);
		
		if(s->w != w || s->h != h){
			Sprite_Reload(s,w,h);
		}
	}
}
void Figure_Render(Figure* f,TransformedView* tv,Pixel* out,unsigned int w,unsigned int h){
	const Vec2 scp = TransformedView_WorldScreenPos(tv,(Vec2){ f->r.p.x,f->r.p.y });
	const Vec2 scd = TransformedView_WorldScreenLength(tv,(Vec2){ f->r.d.x,f->r.d.y });
	
	Figure_Resize(f,(unsigned int)F32_Ceil(scd.x),(unsigned int)F32_Ceil(scd.y));
	
	Sprite* s = Figure_Get_Img(f);
	if(s) RenderSpriteAlpha(s,scp.x,scp.y);
}
void Figure_Free(Figure* f){
	for(int i = 0;i<f->imgs.size;i++){
		Sprite* s = (Sprite*)Vector_Get(&f->imgs,i);
		Sprite_Free(s);
	}
	Vector_Free(&f->imgs);
}



Figure mario;
World world;
TransformedView tv;

Timepoint start;
#define MAX_CHARGE	0.1f
#define MAX_JUMP	15.0f


int Rect_Rect_Compare(const void* e1,const void* e2) {
	Rect r1 = *(Rect*)e1;
	Rect r2 = *(Rect*)e2;
    float d1 = Vec2_Mag(Vec2_Sub(r1.p,mario.r.p));
    float d2 = Vec2_Mag(Vec2_Sub(r2.p,mario.r.p));
    return d1 == d2 ? 0 : (d1 < d2 ? -1 : 1);
}

void Setup(AlxWindow* w){
	tv = TransformedView_New((Vec2){ GetHeight(),GetHeight() });
	TransformedView_Zoom(&tv,(Vec2){ 0.1f,0.1f });
	//TransformedView_Offset(&tv,(Vec2){ -0.5f,0.0f });
	TransformedView_Focus(&tv,&mario.r.p);

	world = World_Make("./data/World/Level1.txt",World_Std_Mapper,(Sprite[]){
		Sprite_Load("./data/Blocks/Void.png"),
		Sprite_Load("./data/Blocks/Dirt.png"),
		Sprite_Load("./data/Blocks/Gras.png"),
		Sprite_Load("./data/Blocks/Brick.png"),
		Sprite_Load("./data/Blocks/Fragezeichen0.png"),
		Sprite_Load("./data/Blocks/Fragezeichen0.png"),
		Sprite_Load("./data/Blocks/Coin0.png"),
		Sprite_Load("./data/Blocks/Podest.png"),
		Sprite_Load("./data/Blocks/Pilz.png"),
		Sprite_Load("./data/Blocks/Open_Block.png"),
		Sprite_None()
	});

	// mario = Figure_Make((Vec2){ 1.0f,25.0f },(Vec2){ 0.5f,1.8f },(Sprite[]){
	// 	Sprite_Load("./data/Sandra/Sandra1_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra1_l.png"),
	// 	Sprite_Load("./data/Sandra/Sandra2_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra2_l.png"),
	// 	Sprite_Load("./data/Sandra/Sandra3_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra3_l.png"),
	// 	Sprite_Load("./data/Sandra/Sandra4_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra4_l.png"),
	// 	Sprite_Load("./data/Sandra/Sandra5_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra5_l.png"),
	// 	Sprite_Load("./data/Sandra/Sandra0_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra0_l.png"),
	// 	Sprite_Load("./data/Sandra/Sandra0_r.png"),
	// 	Sprite_Load("./data/Sandra/Sandra0_l.png"),
	// 	Sprite_None()
	// });

	mario = Figure_Make((Vec2){ 1.0f,25.0f },(Vec2){ 0.9f,0.9f },(Sprite[]){
		Sprite_Load("./data/Mario/Mario0_r.png"),
		Sprite_Load("./data/Mario/Mario0_l.png"),
		Sprite_Load("./data/Mario/Mario1_r.png"),
		Sprite_Load("./data/Mario/Mario1_l.png"),
		Sprite_Load("./data/Mario/Mario2_r.png"),
		Sprite_Load("./data/Mario/Mario2_l.png"),
		Sprite_Load("./data/Mario/Mario3_r.png"),
		Sprite_Load("./data/Mario/Mario3_l.png"),
		Sprite_Load("./data/Mario/Mario4_r.png"),
		Sprite_Load("./data/Mario/Mario4_l.png"),
		Sprite_Load("./data/Mario/Mario5_r.png"),
		Sprite_Load("./data/Mario/Mario5_l.png"),
		Sprite_Load("./data/Mario/Mario6_r.png"),
		Sprite_Load("./data/Mario/Mario6_l.png"),
		Sprite_None()
	});

	start = 0UL;
}
void Update(AlxWindow* w){
	TransformedView_HandlePanZoom(&tv,window.Strokes,GetMouse());

	if(mario.ground){
		if(Stroke(ALX_KEY_W).PRESSED)
			start = Time_Nano();
		if(Stroke(ALX_KEY_W).RELEASED){
			if(start>0){
				float e = Time_Elapsed(start,Time_Nano()) / MAX_CHARGE;
				mario.v.y = -MAX_JUMP * e;
				start = 0UL;
			}
		}
		if(Stroke(ALX_KEY_W).DOWN){
			if(start>0){
				float e = Time_Elapsed(start,Time_Nano());
				if(e > MAX_CHARGE){
					mario.v.y = -MAX_JUMP;
					start = 0UL;
				}
			}
		}
	}

	if(Stroke(ALX_KEY_A).DOWN) 		
		if(mario.ground) 	mario.v.x = -3.0f;
		else 				mario.v.x = -4.0f;
	else if(Stroke(ALX_KEY_D).DOWN) 
		if(mario.ground) 	mario.v.x = 3.0f;
		else 				mario.v.x = 4.0f;
	else
		mario.v.x =  0.0f;

	Figure_Update(&mario,w->ElapsedTime);

	Figure_Collision(&mario,&world,Rect_Rect_Compare);

	Clear(LIGHT_BLUE);

	World_Render(&world,&tv,WINDOW_STD_ARGS);
	Figure_Render(&mario,&tv,WINDOW_STD_ARGS);

	//String str = String_Format("MI:%d",nMaxIterations);
	//CStr_RenderSizeAlxFont(WINDOW_STD_ARGS,&window.AlxFont,str.Memory,str.size,0.0f,0.0f,WHITE);
	//String_Free(&str);
}
void Delete(AlxWindow* w){
	World_Free(&world);
	Figure_Free(&mario);
}

int main(){
    if(Create("Mario",1920,1080,1,1,Setup,Update,Delete))
        Start();
    return 0;
}