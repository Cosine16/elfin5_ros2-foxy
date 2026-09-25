import os
from glob import glob

from setuptools import setup

package_name = 'cos_realsense'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
        (os.path.join('share', package_name, 'urdf'), glob('urdf/*')),
        (os.path.join('share', package_name, 'worlds'), glob('worlds/*')),
        (os.path.join('share', package_name, 'rviz'), glob('rviz/*')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='fit',
    maintainer_email='fit@todo.todo',
    description='D435i point cloud demo for RViz and Gazebo.',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'pointcloud_stats_node = cos_realsense.pointcloud_stats_node:main',
            'heightmap_builder_node = cos_realsense.heightmap_builder_node:main',
            'depth_projector_node = cos_realsense.depth_projector_node:main',
        ],
    },
)
