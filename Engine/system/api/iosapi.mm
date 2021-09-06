#include "iosapi.h"

/*
#import <UIKit/UIKit.h>
#include <string>

using namespace Tempest;

@interface TempestWindow : UIWindow {
  }
@end
@implementation TempestWindow
- (void)layoutSubviews {
  [super layoutSubviews];
  }
@end



@interface ViewController:UIViewController{}
@end

@implementation ViewController
- (void)viewDidLoad {
    [self prefersStatusBarHidden];
  }

- (BOOL)prefersStatusBarHidden {
  return YES;
  }

- (BOOL) shouldAutorotate {
  return YES;
  }

@end

@interface AppDelegate : NSObject <UIApplicationDelegate> {}
@end
@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  (void)application;
  (void)launchOptions;

  CGRect frame = [ [ UIScreen mainScreen ] bounds ];
  TempestWindow *window = [ [ TempestWindow alloc ] initWithFrame: frame];
  window.contentScaleFactor = [UIScreen mainScreen].scale;
  window.rootViewController = [ViewController new];
  window.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  window.backgroundColor = [ UIColor blueColor ];

  [ window makeKeyAndVisible ]; // possible switch here
  return YES;
  }

- (UIInterfaceOrientationMask)application:(UIApplication *)application
  supportedInterfaceOrientationsForWindow:(UIWindow *)window {
    return UIInterfaceOrientationMaskAll;
  }

- (void)applicationWillResignActive:(UIApplication *)application {
    (void)application;
  }


- (void)applicationDidEnterBackground:(UIApplication *)application {
    (void)application;
  }


- (void)applicationWillEnterForeground:(UIApplication *)application {
    (void)application;
  }


- (void)applicationDidBecomeActive:(UIApplication *)application  {
    (void)application;
  }


- (void)applicationWillTerminate:(UIApplication *)application {
    (void)application;
  }

@end


iOSApi::iOSApi() {
  UIApplicationMain(argc, argv, nil, NSStringFromClass([ AppDelegate class ]));
  }
*/
