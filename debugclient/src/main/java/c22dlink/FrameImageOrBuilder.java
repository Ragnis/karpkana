// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: frame.proto

package c22dlink;

public interface FrameImageOrBuilder extends
    // @@protoc_insertion_point(interface_extends:c22dlink.FrameImage)
    com.google.protobuf.MessageOrBuilder {

  /**
   * <code>required int32 sequence = 1;</code>
   */
  boolean hasSequence();
  /**
   * <code>required int32 sequence = 1;</code>
   */
  int getSequence();

  /**
   * <code>required .c22dlink.FrameImage.Type type = 5 [default = ORIGINAL];</code>
   */
  boolean hasType();
  /**
   * <code>required .c22dlink.FrameImage.Type type = 5 [default = ORIGINAL];</code>
   */
  c22dlink.FrameImage.Type getType();

  /**
   * <code>required int32 width = 2;</code>
   */
  boolean hasWidth();
  /**
   * <code>required int32 width = 2;</code>
   */
  int getWidth();

  /**
   * <code>required int32 height = 3;</code>
   */
  boolean hasHeight();
  /**
   * <code>required int32 height = 3;</code>
   */
  int getHeight();

  /**
   * <code>required bytes image = 4;</code>
   */
  boolean hasImage();
  /**
   * <code>required bytes image = 4;</code>
   */
  com.google.protobuf.ByteString getImage();
}
