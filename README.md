# CustomCloth

A plugin trying to make use of cloth simulation algorithm in UE5. Just for fun xD.

## Structure

- UClothMeshComponent
  This component included required data in game thread.

- FClothMeshSceneProxy
  Rendering proxy to collect data from UClothMeshComponent then providing vertex data to RHI Buffer. Must keep sync with UClothMeshComponent. 

  This proxy will be created on component's transform is marked as dirty.

## Reference

1. [手撸物理骨骼系列(2):质点弹簧系统](https://zhuanlan.zhihu.com/p/361126215)

2. [UE4笔记-自定义一个UPrimitiveComponent-3-绘制](https://zhuanlan.zhihu.com/p/450074927)

